// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"
#include "smart_buffer.h"

#include <netinet/in.h>

#include <base/files/scoped_file.h>

// Attempts to create the socket used for accepting connections on the server,
// and if successful returns the file descriptor of the socket.
base::ScopedFD SetupServerSocket();

// Binds the server socket described by |fd| to an address and returns the
// resulting sockaddr_in struct which contains the address.
struct sockaddr_in BindServerSocket(int fd);

// Accepts a new connection to the server described by |fd| and returns the file
// descriptor of the connection.
base::ScopedFD AcceptConnection(int);

// Converts the bound address in |server| and reports it in a human-readable
// format.
void ReportBoundAddress(struct sockaddr_in* server);

void SendBuffer(int sockfd, const SmartBuffer& smart_buffer);
SmartBuffer ReceiveBuffer(int sockfd, size_t size);

// Handles an OpReqDevlist request using |printer| to create an OpRepDevlist
// message.
void HandleDeviceList(int sockfd, const UsbPrinter& printer);

// Reads the requested bus ID for an OpReqImport message. Since we are only
// exporting a single device we should only ever expect to receive the value for
// the exported device, so this function is meant to simply clear the data from
// the socket.
void ReadBusId(int sockfd);

// Handles an OpReqImport request using |printer| to create an OpRepImport
// message.
void HandleAttach(int sockfd, const UsbPrinter& printer);

// Handles either an OpReqDevlist or OpReqImport request received from
// |connection|. Returns whether or not |connection| should remain open. If
// |printer| is successfully attached from the OpReqImport request then set
// |attached| to true.
bool HandleOpRequest(const UsbPrinter& printer, int connection, bool* attached);

// Handles a USB request received from |connection|. Returns whether or not
// |connection| should remain open.
bool HandleUsbRequest(const UsbPrinter& printer, int connection);

// Loops continuously while |connection| remains open and handles any requests
// received on |connection|.
void HandleConnection(const UsbPrinter& printer, base::ScopedFD connection);

// Runs a simple server which processes USBIP requests.
void RunServer(const UsbPrinter& printer);
