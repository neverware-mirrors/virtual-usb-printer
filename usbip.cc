// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usbip.h"

#include "device_descriptors.h"
#include "server.h"
#include "smart_buffer.h"
#include "usb_printer.h"
#include "usbip_constants.h"

#include <base/logging.h>

namespace {

void swap(int* a, int* b) {
  int tmp = *a;
  *a = *b;
  *b = tmp;
}

}  // namespace

void PackUsbip(int* data, size_t msg_size) {
  int size = msg_size / 4;
  for (int i = 0; i < size; i++) {
    data[i] = htonl(data[i]);
  }
  // Put |setup| into network byte order. Since |setup| is a 64-bit integer we
  // have to swap the final 2 int entries since they are both a part of |setup|.
  swap(&data[size - 1], &data[size - 2]);
}

void UnpackUsbip(int* data, size_t msg_size) {
  int size = msg_size / 4;
  for (int i = 0; i < size; i++) {
    data[i] = ntohl(data[i]);
  }
  // Put |setup| into host byte order. Since |setup| is a 64-bit integer we
  // have to swap the final 2 int entries since they are both a part of |setup|.
  swap(&data[size - 1], &data[size - 2]);
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
  printf("usbip setup %llu\n", command.setup);
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

  LOG(INFO) << "Sending response:";
  PrintUsbipRetSubmit(response);
  size_t response_size = sizeof(response);
  PackUsbip((int*)&response, response_size);

  SmartBuffer<uint8_t> smart_buffer(response_size);
  smart_buffer.Add(&response, response_size);
  SendBuffer(sockfd, smart_buffer);
}

void SendUsbControlResponse(int sockfd, const UsbipCmdSubmit& usb_request,
                            const uint8_t* data, size_t data_size) {
  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.actual_length = data_size;

  LOG(INFO) << "Sending response";
  PrintUsbipRetSubmit(response);
  size_t response_size = sizeof(response);
  PackUsbip((int*)&response, response_size);

  SmartBuffer<uint8_t> smart_buffer(response_size);
  smart_buffer.Add(&response, response_size);
  if (data_size > 0) {
    smart_buffer.Add(data, data_size);
  }
  SendBuffer(sockfd, smart_buffer);
}
