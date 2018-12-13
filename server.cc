// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include "device_descriptors.h"
#include "op_commands.h"
#include "smart_buffer.h"
#include "usb_printer.h"
#include "usbip.h"
#include "usbip_constants.h"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <base/files/scoped_file.h>
#include <base/logging.h>

// Opens a socket with the option SO_REUSEADDR and returns the file descriptor
// of the socket.
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

// Creates the sockaddr_in |server| and binds the socket |fd| to it.
sockaddr_in BindServerSocket(int fd) {
  sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(TCP_SERV_PORT);

  if (bind(fd, (sockaddr*)&server, sizeof(server)) < 0) {
    LOG(ERROR) << "Bind error: " << strerror(errno);
    exit(1);
  }

  return server;
}

// Attemps to accept connections on the network socket |fd|.
base::ScopedFD AcceptConnection(int fd) {
  sockaddr_in client;
  socklen_t client_length = sizeof(client);
  int connection =
      accept(fd, reinterpret_cast<sockaddr*>(&client), &client_length);
  if (connection < 0) {
    LOG(ERROR) << "Accept error: " << strerror(errno);
    exit(1);
  }
  LOG(INFO) << "Connection address: " << inet_ntoa(client.sin_addr) << ":"
            << client.sin_port;
  return base::ScopedFD(connection);
}

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

void SendBuffer(int sockfd, const SmartBuffer& smart_buffer) {
  size_t remaining = smart_buffer.size();
  size_t total = 0;
  while (remaining > 0) {
    size_t to_send = std::min(remaining, 512UL);
    ssize_t sent =
        send(sockfd, smart_buffer.data() + total, to_send, MSG_NOSIGNAL);
    CHECK_GE(sent, 0) << "Failed to write data to socket";
    CHECK_GE(remaining, sent) << "Sent more data than expected";
    size_t sent_unsigned = static_cast<size_t>(sent);
    total += sent_unsigned;
    remaining -= sent_unsigned;
    LOG(INFO) << "Sent " << sent_unsigned << " bytes";
  }
  LOG(INFO) << "Sent " << total << " bytes total";
}

SmartBuffer ReceiveBuffer(int sockfd, size_t size) {
  SmartBuffer smart_buffer(size);
  auto buf = std::make_unique<uint8_t[]>(size);
  size_t remaining = size;
  size_t total = 0;
  while (total < size) {
    size_t to_receive = std::min(remaining, 512UL);
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

void HandleDeviceList(int sockfd, const UsbPrinter& printer) {
  OpRepDevlist list;
  LOG(INFO) << "Listing devices...";
  CreateOpRepDevlist(printer.device_descriptor(),
                     printer.configuration_descriptor(),
                     printer.interface_descriptors(), &list);
  SmartBuffer packed_devlist = PackOpRepDevlist(list);
  free(list.interfaces);
  SendBuffer(sockfd, packed_devlist);
}

void ReadBusId(int sockfd) {
  char busid[32];
  LOG(INFO) << "Attaching device...";
  ssize_t received = recv(sockfd, busid, 32, 0);
  if (received != 32) {
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }
}

void HandleAttach(int sockfd, const UsbPrinter& printer) {
  OpRepImport rep;
  CreateOpRepImport(printer.device_descriptor(),
                    printer.configuration_descriptor(), &rep);
  SmartBuffer packed_import = PackOpRepImport(rep);
  SendBuffer(sockfd, packed_import);
}

bool HandleOpRequest(const UsbPrinter& printer, int connection,
                     bool* attached) {
  // Read in the header first in order to determine whether the request is an
  // OpReqDevlist or an OpReqImport.
  OpHeader request;
  ssize_t received = recv(connection, &request, sizeof(request), 0);
  if (received != sizeof(request)) {
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }

  UnpackOpHeader(&request);
  if (request.command == OP_REQ_DEVLIST_CMD) {
    HandleDeviceList(connection, printer);
    return false;
  } else if (request.command == OP_REQ_IMPORT_CMD) {
    ReadBusId(connection);
    HandleAttach(connection, printer);
    *attached = true;
    return true;
  } else {
    LOG(ERROR) << "Unknown command: " << request.command;
    return false;
  }
}

bool HandleUsbRequest(const UsbPrinter& printer, int connection) {
  UsbipCmdSubmit command;
  ssize_t received = recv(connection, &command, sizeof(command), 0);
  if (received != sizeof(command)) {
    if (received == 0) {
      LOG(INFO) << "Client closed connection";
      return false;
    }
    LOG(ERROR) << "Receive error: " << strerror(errno);
    exit(1);
  }

  UnpackUsbip(reinterpret_cast<int*>(&command), sizeof(command));
  PrintUsbipCmdSubmit(command);

  if (command.header.command == COMMAND_USBIP_CMD_SUBMIT) {
    printer.HandleUsbRequest(connection, command);
    return true;
  } else if (command.header.command == COMMAND_USBIP_CMD_UNLINK) {
    LOG(INFO) << "Received unlink URB...shutting down";
    return false;
  } else {
    LOG(ERROR) << "Unknown USBIP command " << command.header.command;
    return false;
  }
}

void HandleConnection(const UsbPrinter& printer, base::ScopedFD connection) {
  bool connection_open = true;
  bool attached = false;
  while (connection_open) {
    if (!attached) {
      LOG(INFO) << "Handling USBIP OP request";
      connection_open = HandleOpRequest(printer, connection.get(), &attached);
    } else {
      LOG(INFO) << "Handling USB request";
      connection_open = HandleUsbRequest(printer, connection.get());
    }
  }
}

void RunServer(const UsbPrinter& printer) {
  base::ScopedFD fd = SetupServerSocket();
  sockaddr_in server = BindServerSocket(fd.get());
  ReportBoundAddress(&server);

  if (listen(fd.get(), SOMAXCONN) < 0) {
    LOG(ERROR) << "Listen error: " << strerror(errno);
    exit(1);
  }

  // Print notification that the server is ready to begin accepting connections.
  printf("virtual-usb-printer: ready to accept connections\n");

  while (true) {
    // Will block until a new connection has been accepted.
    base::ScopedFD connection = AcceptConnection(fd.get());
    HandleConnection(printer, std::move(connection));
  }
}
