// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include "device_descriptors.h"
#include "op_commands.h"
#include "usbip.h"
#include "usbip_constants.h"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <base/logging.h>

namespace {

// Attempts to create the socket used for accepting connections on the server,
// and if successful returns the file descriptor of the socket.
//
// Opens a socket with the option SO_REUSEADDR;
base::ScopedFD SetupServerSocket() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    LOG(ERROR) << "Socket error: " << strerror(errno);
    exit(1);
  }

  int reuse = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    LOG(ERROR) << "setsockopt(SO_REUSEADDR) failed";
  }

  return base::ScopedFD(fd);
}

// Binds the server socket described by |fd| to an address and returns the
// resulting sockaddr_in struct which contains the address.
sockaddr_in BindServerSocket(const base::ScopedFD& sockfd) {
  sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(TCP_SERV_PORT);

  sockaddr* server_socket = reinterpret_cast<sockaddr*>(&server);
  if (bind(sockfd.get(), server_socket, sizeof(server)) < 0) {
    LOG(ERROR) << "Bind error: " << strerror(errno);
    exit(1);
  }

  return server;
}

// Accepts a new connection to the server described by |fd| and returns the file
// descriptor of the connection.
base::ScopedFD AcceptConnection(const base::ScopedFD& fd) {
  sockaddr_in client;
  socklen_t client_length = sizeof(client);
  int connection =
      accept(fd.get(), reinterpret_cast<sockaddr*>(&client), &client_length);
  if (connection < 0) {
    LOG(ERROR) << "Accept error: " << strerror(errno);
    exit(1);
  }
  LOG(INFO) << "Connection address: " << inet_ntoa(client.sin_addr) << ":"
            << client.sin_port;
  return base::ScopedFD(connection);
}

// Converts the bound address in |server| and reports it in a human-readable
// format.
void ReportBoundAddress(sockaddr_in* server) {
  char address[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &server->sin_addr, address, INET_ADDRSTRLEN)) {
    LOG(INFO) << "Bound server to address " << address << ":"
              << server->sin_port;
  } else {
    LOG(ERROR) << "inet_ntop: " << strerror(errno);
    exit(1);
  }
}

// Reads the requested bus ID for an OpReqImport message. Since we are only
// exporting a single device we should only ever expect to receive the value for
// the exported device, so this function is meant to simply clear the data from
// the socket.
void ReadBusId(const base::ScopedFD& sockfd) {
  char busid[32];
  LOG(INFO) << "Attaching device...";
  ssize_t received = recv(sockfd.get(), busid, 32, 0);
  if (received != 32) {
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }
}

}  // namespace

void SendBuffer(int sockfd, const SmartBuffer& smart_buffer) {
  size_t remaining = smart_buffer.size();
  size_t total = 0;
  while (remaining > 0) {
    size_t to_send = std::min<size_t>(remaining, 512UL);
    ssize_t sent =
        send(sockfd, smart_buffer.data() + total, to_send, MSG_NOSIGNAL);
    CHECK_GE(sent, 0) << "Failed to write data to socket";
    CHECK_GE(remaining, sent) << "Sent more data than expected";
    size_t sent_unsigned = static_cast<size_t>(sent);
    total += sent_unsigned;
    remaining -= sent_unsigned;
  }
}

SmartBuffer ReceiveBuffer(int sockfd, size_t size) {
  SmartBuffer smart_buffer(size);
  auto buf = std::make_unique<uint8_t[]>(size);
  size_t remaining = size;
  size_t total = 0;
  while (total < size) {
    size_t to_receive = std::min<size_t>(remaining, 512UL);
    ssize_t received = recv(sockfd, buf.get() + total, to_receive, 0);
    CHECK_GE(received, 0) << "Failed to receive data from socket";
    CHECK_GE(remaining, received) << "Received more data than expected";
    size_t received_unsigned = static_cast<size_t>(received);
    total += received_unsigned;
    remaining -= received_unsigned;
  }
  smart_buffer.Add(buf.get(), size);
  return smart_buffer;
}

Server::Server(UsbPrinter printer) : printer_(std::move(printer)) {}

void Server::Run() {
  base::ScopedFD fd = SetupServerSocket();
  sockaddr_in server = BindServerSocket(fd);
  ReportBoundAddress(&server);

  if (listen(fd.get(), SOMAXCONN) < 0) {
    LOG(ERROR) << "Listen error: " << strerror(errno);
    exit(1);
  }

  // Print notification that the server is ready to begin accepting connections.
  printf("virtual-usb-printer: ready to accept connections\n");

  while (true) {
    // Will block until a new connection has been accepted.
    base::ScopedFD connection = AcceptConnection(fd);
    HandleConnection(std::move(connection));
  }
}

void Server::HandleDeviceList(const base::ScopedFD& connection) const {
  OpRepDevlist list;
  LOG(INFO) << "Listing devices...";
  CreateOpRepDevlist(printer_.device_descriptor(),
                     printer_.configuration_descriptor(),
                     printer_.interface_descriptors(), &list);
  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  free(list.interfaces);
  SendBuffer(connection.get(), packed_devlist);
}

void Server::HandleAttach(const base::ScopedFD& connection) const {
  OpRepImport rep;
  CreateOpRepImport(printer_.device_descriptor(),
                    printer_.configuration_descriptor(), &rep);
  SmartBuffer packed_import = PackOpRepImport(rep);
  SendBuffer(connection.get(), packed_import);
}

bool Server::HandleOpRequest(const base::ScopedFD& connection,
                             bool* attached) const {
  // Read in the header first in order to determine whether the request is an
  // OpReqDevlist or an OpReqImport.
  OpHeader request;
  ssize_t received = recv(connection.get(), &request, sizeof(request), 0);
  if (received != sizeof(request)) {
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }

  UnpackOpHeader(&request);
  if (request.command == OP_REQ_DEVLIST_CMD) {
    HandleDeviceList(connection);
    return false;
  } else if (request.command == OP_REQ_IMPORT_CMD) {
    ReadBusId(connection);
    HandleAttach(connection);
    *attached = true;
    return true;
  } else {
    LOG(ERROR) << "Unknown command: " << request.command;
    return false;
  }
}

bool Server::HandleUsbRequest(const base::ScopedFD& connection) {
  UsbipCmdSubmit command;
  ssize_t received = recv(connection.get(), &command, sizeof(command), 0);
  if (received != sizeof(command)) {
    if (received == 0) {
      LOG(INFO) << "Client closed connection";
      return false;
    }
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }

  UnpackUsbip(reinterpret_cast<int*>(&command), sizeof(command));
  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    printer_.HandleUsbRequest(connection.get(), command);
    return true;
  } else if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
    LOG(INFO) << "Received unlink URB...ignoring";
    LOG(INFO) << "Unlinked seqnum : " << command.transfer_flags;
    return true;
  } else {
    LOG(ERROR) << "Unknown USBIP command " << command.header.command;
    return false;
  }
}

void Server::HandleConnection(base::ScopedFD connection) {
  bool connection_open = true;
  bool attached = false;
  while (connection_open) {
    if (!attached) {
      connection_open = HandleOpRequest(connection, &attached);
    } else {
      connection_open = HandleUsbRequest(connection);
    }
  }
}
