// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_USBIP_H__
#define __USBIP_USBIP_H__

/*
 * This file defines the supported messages from the usbip-core protocol,
 * and some utility functions for processing them.
 *
 * In the context of the defined messages:
 *   "Cmd" is used in messages that submit a request.
 *   "Ret" is used in messages that respond to a request.
 *
 * For more information about the usbip protocol refer to the following
 * documentation:
 * https://www.kernel.org/doc/Documentation/usb/usbip_protocol.txt
 * https://en.opensuse.org/SDB:USBIP
 */

#include <cstdlib>
#include <vector>

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

// Common USBIP header used in both requests and responses.
struct UsbipHeaderBasic {
  int command;    // The USBIP request type.
  int seqnum;     // Sequential number that identifies requests.
  int devid;      // Specifies a remote USB devie uniquely.
  int direction;  // Direction of the transfer (0 Out, 1 In).
  int ep;         // The USB endpoint number.
};

// Used to submit a USB request.
struct UsbipCmdSubmit {
  UsbipHeaderBasic header;
  int transfer_flags;          // URB flags.
  int transfer_buffer_length;  // Data size for transfer.
  int start_frame;             // Initial frame for iso or interrupt transfers.
  int number_of_packets;       // Number of iso packets.
  int interval;                // Timeout for response.
  uint64_t setup;              // Contains a USB SETUP packet.
};

// Used to reply to a USB request.
struct UsbipRetSubmit {
  UsbipHeaderBasic header;
  int status;         // Response status (O for success, non-zero for error).
  int actual_length;  // Number of bytes transferred.
  int start_frame;    // Iniitial frame for iso or interrupt transfers.
  int number_of_packets;  // Number of iso packets.
  int error_count;        // Number of errors for iso transfers.
  uint64_t setup;         // Contains a USB SETUP packet.
};

// Represents a USB SETUP packet.
struct UsbControlRequest {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint8_t wValue0;
  uint8_t wValue1;
  uint8_t wIndex0;
  uint8_t wIndex1;
  uint16_t wLength;
};

// Prints the contents of various elements of USBIP messages for debugging
// purposes.
void PrintUsbHeaderBasic(const UsbipHeaderBasic& header);
void PrintUsbipCmdSubmit(const UsbipCmdSubmit& command);
void PrintUsbipRetSubmit(const UsbipRetSubmit& response);
void PrintUsbControlRequest(const UsbControlRequest& request);

// Creates a new UsbipRetSubmit which is initialized using the shared values
// from |request|.
UsbipRetSubmit CreateUsbipRetSubmit(const UsbipCmdSubmit& usb_request);

// Converts the contents of a UsbipCmdSubmit message into network byte order.
void PackUsbip(int* data, size_t msg_size);

// Converts the contents a UsbipCmdSubmit message into host uint8_t order.
void UnpackUsbip(int* data, size_t msg_size);

// Responds to the USB data request |usb_request| by sending a UsbRetSubmit
// message that uses |received| to indicate how many uint8_ts that it
// successfully received.
void SendUsbDataResponse(int sockfd, const UsbipCmdSubmit& usb_request,
                         size_t received);

// Sends a UsbipRetSubmit message to the socket described by |sockfd|.
// |usb_request| is used to create the UsbipRetSubmit header and |data|
// contains the actual URB data.
void SendUsbControlResponse(int sockfd, const UsbipCmdSubmit& usb_request,
                            const uint8_t* data, size_t size);

#endif  // __USBIP_USBIP_H__
