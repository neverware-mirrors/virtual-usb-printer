// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"

#include "device_descriptors.h"
#include "server.h"
#include "smart_buffer.h"
#include "usbip.h"
#include "usbip_constants.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <arpa/inet.h>
#include <cups/cups.h>
#include <sys/socket.h>
#include <unistd.h>

#include <base/logging.h>

namespace {

// Bitmask constants used for extracting individual values out of
// UsbControlRequest which is packed inside of a uint64_t.
const uint64_t REQUEST_TYPE_MASK = (0xFFUL << 56);
const uint64_t REQUEST_MASK = (0xFFUL << 48);
const uint64_t VALUE_0_MASK = (0xFFUL << 40);
const uint64_t VALUE_1_MASK = (0xFFUL << 32);
const uint64_t INDEX_0_MASK = (0xFFUL << 24);
const uint64_t INDEX_1_MASK = (0xFFUL << 16);
const uint64_t LENGTH_MASK = 0xFFFFUL;

// Returns the numeric value of the "type" stored within the |bmRequestType|
// bitmap.
int GetControlType(uint8_t bmRequestType) {
  // The "type" of the request is stored within bits 5 and 6 of |bmRequestType|.
  // So we shift these bits down to the least significant bits and perform a
  // bitwise AND operation in order to clear any other bits and return strictly
  // the number value of the type bits.
  return (bmRequestType >> 5) & 3;
}

// Unpacks the standard USB SETUP packet contained within |setup| into a
// UsbControlRequest struct and returns the result.
UsbControlRequest CreateUsbControlRequest(long long setup) {
  UsbControlRequest request;
  request.bmRequestType = (setup & REQUEST_TYPE_MASK) >> 56;
  request.bRequest = (setup & REQUEST_MASK) >> 48;
  request.wValue0 = (setup & VALUE_0_MASK) >> 40;
  request.wValue1 = (setup & VALUE_1_MASK) >> 32;
  request.wIndex0 = (setup & INDEX_0_MASK) >> 24;
  request.wIndex1 = (setup & INDEX_1_MASK) >> 16;
  request.wLength = ntohs(setup & LENGTH_MASK);
  return request;
}

}  // namespace

// explicit
UsbPrinter::UsbPrinter(
    const UsbDeviceDescriptor& device_descriptor,
    const UsbConfigurationDescriptor& configuration_descriptor,
    const UsbDeviceQualifierDescriptor& qualifier_descriptor,
    const std::vector<std::vector<char>>& strings,
    const std::vector<char> ieee_device_id,
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>& endpoints)
    : device_descriptor_(device_descriptor),
      configuration_descriptor_(configuration_descriptor),
      qualifier_descriptor_(qualifier_descriptor),
      strings_(strings),
      ieee_device_id_(ieee_device_id),
      interfaces_(interfaces),
      endpoints_(endpoints) {}

void UsbPrinter::HandleUsbRequest(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.header.ep == 0) {
    LOG(INFO) << "# Control Request";
    HandleUsbControl(sockfd, usb_request);
  } else {
    LOG(INFO) << "# Data Request";
    HandleUsbData(sockfd, usb_request);
  }
}

void UsbPrinter::HandleUsbControl(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
  PrintUsbControlRequest(control_request);
  int request_type = GetControlType(control_request.bmRequestType);
  switch (request_type) {
    case STANDARD_TYPE:
      HandleStandardControl(sockfd, usb_request, control_request);
      break;
    case CLASS_TYPE:
      HandlePrinterControl(sockfd, usb_request, control_request);
      break;
    case VENDOR_TYPE:
    case RESERVED_TYPE:
    default:
      LOG(ERROR) << "Unable to handle request of type: " << request_type;
      break;
  }
}

void UsbPrinter::HandleUsbData(int sockfd,
                               const UsbipCmdSubmit& usb_request) const {
  SmartBuffer smart_buffer =
      ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = smart_buffer.size();
  LOG(INFO) << "Received " << received << " bytes";
  SendUsbDataResponse(sockfd, usb_request, received);
}

void UsbPrinter::HandleStandardControl(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_STATUS:
      HandleGetStatus(sockfd, usb_request, control_request);
      break;
    case GET_DESCRIPTOR:
      HandleGetDescriptor(sockfd, usb_request, control_request);
      break;
    case SET_DESCRIPTOR:
      break;
    case GET_CONFIGURATION:
      HandleGetConfiguration(sockfd, usb_request, control_request);
      break;
    case SET_CONFIGURATION:
      HandleSetConfiguration(sockfd, usb_request, control_request);
      break;
    case GET_INTERFACE:
      // Support for this will be needed for interfaces with alt settings.
      break;
    case SET_INTERFACE:
      HandleSetInterface(sockfd, usb_request, control_request);
      break;
    default:
      break;
  }
}

void UsbPrinter::HandlePrinterControl(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  switch (control_request.bRequest) {
    case GET_DEVICE_ID:
      HandleGetDeviceId(sockfd, usb_request, control_request);
      break;
    case GET_PORT_STATUS:
      break;
    case SOFT_RESET:
      break;
    default:
      LOG(ERROR) << "Unkown printer class request " << control_request.bRequest;
  }
}

void UsbPrinter::HandleGetStatus(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetStatus %u[%u]\n", control_request.wValue1,
         control_request.wValue0);
  uint16_t status = 0;
  SmartBuffer response(sizeof(status));
  response.Add(&status, sizeof(status));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  switch (control_request.wValue1) {
    case USB_DESCRIPTOR_DEVICE:
      HandleGetDeviceDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_CONFIGURATION:
      HandleGetConfigurationDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_STRING:
      HandleGetStringDescriptor(sockfd, usb_request, control_request);
      break;
    case USB_DESCRIPTOR_INTERFACE:
      break;
    case USB_DESCRIPTOR_ENDPOINT:
      break;
    case USB_DESCRIPTOR_DEVICE_QUALIFIER:
      HandleGetDeviceQualifierDescriptor(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Unknown descriptor type request: "
                 << control_request.wValue1;
  }
}

void UsbPrinter::HandleGetDeviceDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetDeviceDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(sizeof(device_descriptor_));
  response.Add(&device_descriptor_, sizeof(device_descriptor_));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfigurationDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetConfigurationDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(control_request.wLength);
  response.Add(&configuration_descriptor_, sizeof(configuration_descriptor_));

  if (control_request.wLength == sizeof(configuration_descriptor_)) {
    // Only the configuration descriptor itself has been requested.
    printf("Only configuration descriptor requested\n");
    SendUsbControlResponse(sockfd, usb_request, response.data(),
                           response.size());
    return;
  }

  // Place each interface and their corresponding endnpoint descriptors into the
  // response buffer.
  for (int i = 0; i < configuration_descriptor_.bNumInterfaces; ++i) {
    const auto& interface = interfaces_[i];
    response.Add(&interface, sizeof(interface));
    auto iter = endpoints_.find(interface.bInterfaceNumber);
    if (iter == endpoints_.end()) {
      LOG(ERROR) << "Unable to find endpoints for interface "
                 << interface.bInterfaceNumber;
      exit(1);
    }
    for (const auto& endpoint : iter->second) {
      response.Add(&endpoint, sizeof(endpoint));
    }
  }

  CHECK_EQ(control_request.wLength, response.size())
      << "Response length does not match requested number of bytes";
  // After filling the buffer with all of the necessary descriptors we can send
  // a response.
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetDeviceQualifierDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetDeviceQualifierDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(sizeof(qualifier_descriptor_));
  response.Add(&qualifier_descriptor_, sizeof(qualifier_descriptor_));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetStringDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetStringDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  int index = control_request.wValue0;
  SmartBuffer response(strings_[index][0]);
  response.Add(strings_[index].data(), strings_[index][0]);
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfiguration(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetConfiguration %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  // Note: For now we only have one configuration set, so we just respond with
  // with |configuration_descriptor_.bConfigurationValue|.
  SmartBuffer response(sizeof(configuration_descriptor_.bConfigurationValue));
  response.Add(&configuration_descriptor_.bConfigurationValue,
               sizeof(configuration_descriptor_.bConfigurationValue));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleSetConfiguration(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleSetConfiguration %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  // NOTE: For now we have only one configuration to set, so we just respond
  // with an empty message as a confirmation.
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void UsbPrinter::HandleSetInterface(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleSetInterface %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  // NOTE: For now we have only one interface to set, so we just respond
  // with an empty message as a confirmation.
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void UsbPrinter::HandleGetDeviceId(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetDeviceId %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(ieee_device_id_.size());
  response.Add(ieee_device_id_.data(), ieee_device_id_.size());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}
