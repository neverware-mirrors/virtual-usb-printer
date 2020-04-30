// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_SERVER_H__
#define __USBIP_SERVER_H__

#include <netinet/in.h>

#include <base/files/scoped_file.h>

#include "usb_printer.h"
#include "smart_buffer.h"

void SendBuffer(int sockfd, const SmartBuffer& smart_buffer);
SmartBuffer ReceiveBuffer(int sockfd, size_t size);

class Server {
 public:
  // Create a simple server which processes USBIP requests.
  explicit Server(UsbPrinter printer);

  // Run the server to process USBIP requests.
  // This function does not return.
  void Run();

 private:
  // Handles an OpReqDevlist request using |printer_| to create an OpRepDevlist
  // message.
  void HandleDeviceList(const base::ScopedFD& connection) const;

  // Handles an OpReqImport request using |printer_| to create an OpRepImport
  // message.
  void HandleAttach(const base::ScopedFD& connection) const;

  // Handles either an OpReqDevlist or OpReqImport request received from
  // |connection|. Returns whether or not |connection| should remain open. If
  // |printer_| is successfully attached from the OpReqImport request then set
  // |attached| to true.
  bool HandleOpRequest(const base::ScopedFD& connection, bool* attached) const;

  // Handles a USB request received from |connection|. Returns whether or not
  // |connection| should remain open.
  bool HandleUsbRequest(const base::ScopedFD& connection);

  // Loops continuously while |connection| remains open and handles any requests
  // received on |connection|.
  void HandleConnection(base::ScopedFD connection);

  UsbPrinter printer_;
};

#endif  // __USBIP_SERVER_H__
