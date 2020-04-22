// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip.h"

#include <cinttypes>

#include "device_descriptors.h"
#include "server.h"
#include "smart_buffer.h"
#include "usb_printer.h"
#include "usbip_constants.h"

#include <base/logging.h>

SmartBuffer PackUsbipRetSubmit(const UsbipRetSubmit& reply) {
  SmartBuffer serialized;
  serialized.Add(htonl(reply.header.command));
  serialized.Add(htonl(reply.header.seqnum));
  serialized.Add(htonl(reply.header.devid));
  serialized.Add(htonl(reply.header.direction));
  serialized.Add(htonl(reply.header.ep));

  serialized.Add(htonl(reply.status));
  serialized.Add(htonl(reply.actual_length));
  serialized.Add(htonl(reply.start_frame));
  serialized.Add(htonl(reply.number_of_packets));
  serialized.Add(htonl(reply.error_count));

  serialized.Add(htobe64(reply.setup));
  return serialized;
}

UsbipCmdSubmit UnpackUsbipCmdSubmit(SmartBuffer* buf) {
  UsbipCmdSubmit result;
  CHECK(buf->size() >= sizeof(result));
  memcpy(&result, buf->data(), sizeof(result));
  buf->Erase(0, sizeof(result));

  result.header.command = ntohl(result.header.command);
  result.header.seqnum = ntohl(result.header.seqnum);
  result.header.devid = ntohl(result.header.devid);
  result.header.direction = ntohl(result.header.direction);
  result.header.ep = ntohl(result.header.ep);

  result.transfer_flags = ntohl(result.transfer_flags);
  result.transfer_buffer_length = ntohl(result.transfer_buffer_length);
  result.start_frame = ntohl(result.start_frame);
  result.number_of_packets = ntohl(result.number_of_packets);
  result.interval = ntohl(result.interval);
  result.setup = be64toh(result.setup);
  return result;
}

void PrintUsbipHeaderBasic(const UsbipHeaderBasic& header) {
  printf("usbip cmd %u\n", header.command);
  printf("usbip seqnum %u\n", header.seqnum);
  printf("usbip devid %u\n", header.devid);
  printf("usbip direction %u\n", header.direction);
  printf("usbip ep %u\n", header.ep);
}

void PrintUsbipCmdSubmit(const UsbipCmdSubmit& command) {
  PrintUsbipHeaderBasic(command.header);
  printf("usbip flags %u\n", command.transfer_flags);
  printf("usbip number of packets %u\n", command.number_of_packets);
  printf("usbip interval %u\n", command.interval);
  printf("usbip setup %" PRIu64 "\n", command.setup);
  printf("usbip buffer length  %u\n", command.transfer_buffer_length);
}

void PrintUsbipRetSubmit(const UsbipRetSubmit& response) {
  PrintUsbipHeaderBasic(response.header);
  printf("usbip status %u\n", response.status);
  printf("usbip actual_length %u\n", response.actual_length);
  printf("usbip start_frame %u\n", response.start_frame);
  printf("usbip number_of_packets %u\n", response.number_of_packets);
  printf("usbip error_count %u\n", response.error_count);
}

void PrintUsbControlRequest(const UsbControlRequest& request) {
  printf("  UC Request Type %u\n", request.bmRequestType);
  printf("  UC Request %u\n", request.bRequest);
  printf("  UC Value  %u[%u]\n", request.wValue1, request.wValue0);
  printf("  UC Index  %u-%u\n", request.wIndex1, request.wIndex0);
  printf("  UC Length %u\n", request.wLength);
}

UsbipRetSubmit CreateUsbipRetSubmit(const UsbipCmdSubmit& request) {
  UsbipRetSubmit response;
  memset(&response, 0, sizeof(response));
  response.header.command = COMMAND_USBIP_RET_SUBMIT;
  response.header.seqnum = request.header.seqnum;
  response.header.devid = request.header.devid;
  response.header.direction = request.header.direction;
  response.header.ep = request.header.ep;
  return response;
}

void SendUsbDataResponse(int sockfd, const UsbipCmdSubmit& usb_request,
                         size_t received) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = received;

  PrintUsbipRetSubmit(response);
  SendBuffer(sockfd, PackUsbipRetSubmit(response));
}

void SendUsbControlResponse(int sockfd, const UsbipCmdSubmit& usb_request,
                            const uint8_t* data, size_t data_size) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = data_size;

  PrintUsbipRetSubmit(response);
  SmartBuffer smart_buffer = PackUsbipRetSubmit(response);
  if (data_size > 0) {
    smart_buffer.Add(data, data_size);
  }
  SendBuffer(sockfd, smart_buffer);
}
