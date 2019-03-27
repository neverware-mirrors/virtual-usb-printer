// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_printer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "device_descriptors.h"
#include "ipp_util.h"
#include "server.h"
#include "smart_buffer.h"
#include "usbip.h"
#include "usbip_constants.h"

namespace {

// Represents the success code for an IPP response.
const uint16_t kSuccessStatus = 0;

// Bitmask constants used for extracting individual values out of
// UsbControlRequest which is packed inside of a uint64_t.
const uint64_t REQUEST_TYPE_MASK{0xFFULL << 56};
const uint64_t REQUEST_MASK{0xFFULL << 48};
const uint64_t VALUE_0_MASK{0xFFULL << 40};
const uint64_t VALUE_1_MASK{0xFFULL << 32};
const uint64_t INDEX_0_MASK{0xFFUL << 24};
const uint64_t INDEX_1_MASK{0xFFUL << 16};
const uint64_t LENGTH_MASK{0xFFFFUL};

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

void InterfaceManager::QueueMessage(const SmartBuffer& message) {
  queue_.push(message);
}

bool InterfaceManager::QueueEmpty() const {
  return queue_.empty();
}

void InterfaceManager::ChunkedMessageAdd(const SmartBuffer& message) {
  chunked_message_.Add(message);
}

SmartBuffer InterfaceManager::PopMessage() {
  CHECK(!QueueEmpty()) << "Can't pop message from empty queue.";
  auto message = queue_.front();
  queue_.pop();
  return message;
}

// explicit
UsbDescriptors::UsbDescriptors(
    const UsbDeviceDescriptor& device_descriptor,
    const UsbConfigurationDescriptor& configuration_descriptor,
    const UsbDeviceQualifierDescriptor& qualifier_descriptor,
    const std::vector<std::vector<char>>& string_descriptors,
    const std::vector<char>& ieee_device_id,
    const std::vector<UsbInterfaceDescriptor>& interface_descriptors,
    const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>&
        endpoint_descriptors)
    : device_descriptor_(device_descriptor),
      configuration_descriptor_(configuration_descriptor),
      qualifier_descriptor_(qualifier_descriptor),
      string_descriptors_(string_descriptors),
      ieee_device_id_(ieee_device_id),
      interface_descriptors_(interface_descriptors),
      endpoint_descriptors_(endpoint_descriptors) {}

// explicit
UsbPrinter::UsbPrinter(const UsbDescriptors& usb_descriptors)
    : usb_descriptors_(usb_descriptors),
      record_document_(false),
      interface_managers_(usb_descriptors.interface_descriptors().size()) {}

bool UsbPrinter::IsIppUsb() const {
  int count = 0;
  const std::vector<UsbInterfaceDescriptor> interfaces =
      interface_descriptors();
  for (const auto& interface : interfaces) {
    if (interface.bInterfaceClass == 7 && interface.bInterfaceSubClass == 1 &&
        interface.bInterfaceProtocol == 4) {
      ++count;
    }
  }
  // An ipp-over-usb printer must have at least 2 ipp-over-usb interfaces.
  return count >= 2;
}

void UsbPrinter::HandleUsbRequest(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  // Endpoint 0 is used for USB control requests.
  if (usb_request.header.ep == 0) {
    HandleUsbControl(sockfd, usb_request);
  } else {
    if (usb_request.header.direction == 1) {
      HandleBulkInRequest(sockfd, usb_request);
    } else {
      if (IsIppUsb()) {
        HandleIppUsbData(sockfd, usb_request);
      } else {
        HandleUsbData(sockfd, usb_request);
      }
    }
  }
}

void UsbPrinter::HandleUsbControl(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  UsbControlRequest control_request =
      CreateUsbControlRequest(usb_request.setup);
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
  SmartBuffer data = ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = data.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbDataResponse(sockfd, usb_request, received);
  if (record_document_) {
    LOG(INFO) << "Recording document...";
    WriteDocument(data);
  }
}

void UsbPrinter::HandleIppUsbData(int sockfd,
                                  const UsbipCmdSubmit& usb_request) const {
  SmartBuffer message =
      ReceiveBuffer(sockfd, usb_request.transfer_buffer_length);
  size_t received = message.size();
  LOG(INFO) << "Received " << received << " bytes";
  // Acknowledge receipt of BULK transfer.
  SendUsbDataResponse(sockfd, usb_request, received);

  if (IsHttpChunkedMessage(message)) {
    CHECK(!GetReceivingChunked(usb_request.header.ep))
        << "Received new chunked message before previous chunked message could "
           "be completed";
    SetReceivingChunked(usb_request.header.ep, true);
  }

  if (GetReceivingChunked(usb_request.header.ep)) {
    HandleChunkedIppUsbData(sockfd, usb_request, &message);
  } else {
    // If we receive an HTTP message with no body, then skip it.
    if (!ContainsHttpBody(message)) {
      return;
    }
    IppHeader ipp_header = GetIppHeader(message);
    HandleIppUsbData(sockfd, usb_request, ipp_header);
  }
}

void UsbPrinter::HandleChunkedIppUsbData(int sockfd,
                                         const UsbipCmdSubmit& usb_request,
                                         SmartBuffer* message) const {
  int endpoint = usb_request.header.ep;
  if (IsHttpChunkedMessage(*message)) {
    // Exit in the case that |message| only contains the HTTP header.
    if (!ContainsHttpBody(*message)) {
      return;
    }
    SetChunkedIppHeader(endpoint, GetIppHeader(*message));
    RemoveHttpHeader(message);
  }

  bool complete = ContainsFinalChunk(*message);
  SetReceivingChunked(endpoint, !complete);
  ChunkedMessageAdd(endpoint, *message);

  if (complete) {
    // The final chunk has been received.
    if (record_document_) {
      SmartBuffer* messages = GetChunkedMessage(endpoint);
      SetIppMessage(endpoint, ExtractIppMessage(messages));
      SetDocument(endpoint, MergeDocument(messages));
      LOG(INFO) << "Recording document...";
      WriteDocument(GetDocument(endpoint));
    }
    const IppHeader& chunked_header = GetChunkedIppHeader(endpoint);
    HandleIppUsbData(sockfd, usb_request, chunked_header);
  }
}

// TODO(valleau): Rename this function
void UsbPrinter::HandleIppUsbData(int sockfd, const UsbipCmdSubmit& usb_request,
                                  const IppHeader& ipp_header) const {
  switch (ipp_header.operation_id) {
    case IPP_VALIDATE_JOB:
      HandleValidateJob(usb_request, ipp_header);
      break;
    case IPP_CREATE_JOB:
      HandleCreateJob(usb_request, ipp_header);
      break;
    case IPP_SEND_DOCUMENT:
      HandleSendDocument(usb_request, ipp_header);
      break;
    case IPP_GET_JOB_ATTRIBUTES:
      HandleGetJobAttributes(usb_request, ipp_header);
      break;
    case IPP_GET_PRINTER_ATTRIBUTES:
      HandleGetPrinterAttributes(sockfd, usb_request, ipp_header);
      break;
    default:
      LOG(ERROR) << "Unknown operation id in ipp request "
                 << ipp_header.operation_id;
  }
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
    case GET_CONFIGURATION:
      HandleGetConfiguration(sockfd, usb_request, control_request);
      break;
    case CLEAR_FEATURE:
    case SET_FEATURE:
    case SET_ADDRESS:
    case SET_DESCRIPTOR:
    case SET_CONFIGURATION:
    case GET_INTERFACE:
    case SET_INTERFACE:
    case SET_FRAME:
      HandleUnsupportedRequest(sockfd, usb_request, control_request);
      break;
    default:
      LOG(ERROR) << "Received unknown control request "
                 << control_request.bRequest;
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

bool UsbPrinter::SetupRecordDocument(const std::string& path) {
  record_document_ = true;
  record_document_path_ = path;
  record_document_file_.Initialize(
      base::FilePath(path), base::File::FLAG_CREATE | base::File::FLAG_APPEND);
  return record_document_file_.IsValid();
}

base::File::Error UsbPrinter::FileError() const {
  return record_document_file_.error_details();
}

void UsbPrinter::QueueMessage(int ep, const SmartBuffer& message) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].QueueMessage(message);
}

SmartBuffer UsbPrinter::PopMessage(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].PopMessage();
}

bool UsbPrinter::QueueEmpty(int ep) const  {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].QueueEmpty();
}

void UsbPrinter::SetReceivingChunked(int ep, bool b) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].SetReceivingChunked(b);
}

bool UsbPrinter::GetReceivingChunked(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].receiving_chunked();
}

void UsbPrinter::SetChunkedIppHeader(int ep,
                                     const IppHeader& ipp_header) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].SetChunkedIppHeader(ipp_header);
}

void UsbPrinter::SetIppMessage(int ep, const SmartBuffer& ipp_message) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].SetIppMessage(ipp_message);
}

void UsbPrinter::SetDocument(int ep, const SmartBuffer& document) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].SetDocument(document);
}

const IppHeader& UsbPrinter::GetChunkedIppHeader(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].chunked_ipp_header();
}

void UsbPrinter::ChunkedMessageAdd(int ep, const SmartBuffer& message) const {
  int index = GetInterfaceManagerIndex(ep);
  interface_managers_[index].ChunkedMessageAdd(message);
}

SmartBuffer* UsbPrinter::GetChunkedMessage(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].chunked_message();
}

int UsbPrinter::GetInterfaceManagerIndex(int ep) const {
  CHECK_GT(ep, 0) << "Received request on an invalid endpoint";
  // Since each interface contains a pair of in/out endpoints, we perform this
  // conversion in order to retrieve the corresponding interface number.
  // Examples:
  //   endpoints 1 and 2 both map to interface 0.
  //   endpoints 3 and 4 both map to interface 1.
  int index = (ep - 1) / 2;
  CHECK_LT(index, interface_managers_.size())
      << "Received request on an invalid endpoint";
  return index;
}

const SmartBuffer& UsbPrinter::GetIppMessage(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].ipp_message();
}

const SmartBuffer& UsbPrinter::GetDocument(int ep) const {
  int index = GetInterfaceManagerIndex(ep);
  return interface_managers_[index].document();
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

  SmartBuffer response(sizeof(device_descriptor()));
  const UsbDeviceDescriptor& dev = device_descriptor();

  // If the requested number of bytes is smaller than the size of the device
  // descriptor then only send a portion of the descriptor.
  if (control_request.wLength < sizeof(dev)) {
    response.Add(&dev, control_request.wLength);
  } else {
    response.Add(dev);
  }

  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfigurationDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetConfigurationDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(control_request.wLength);
  response.Add(configuration_descriptor());

  if (control_request.wLength == sizeof(configuration_descriptor())) {
    // Only the configuration descriptor itself has been requested.
    printf("Only configuration descriptor requested\n");
    SendUsbControlResponse(sockfd, usb_request, response.data(),
                           response.size());
    return;
  }

  const auto& interfaces = interface_descriptors();
  const auto& endpoints = endpoint_descriptors();

  // Place each interface and their corresponding endnpoint descriptors into the
  // response buffer.
  for (int i = 0; i < configuration_descriptor().bNumInterfaces; ++i) {
    const auto& interface = interfaces[i];
    response.Add(&interface, sizeof(interface));
    auto iter = endpoints.find(interface.bInterfaceNumber);
    if (iter == endpoints.end()) {
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

  SmartBuffer response(sizeof(qualifier_descriptor()));
  response.Add(qualifier_descriptor());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetStringDescriptor(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetStringDescriptor %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  int index = control_request.wValue0;
  const auto& strings = string_descriptors();
  SmartBuffer response(strings[index][0]);
  response.Add(strings[index].data(), strings[index][0]);
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleGetConfiguration(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetConfiguration %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  // Note: For now we only have one configuration set, so we just respond with
  // with |configuration_descriptor_.bConfigurationValue|.
  const auto& configuration = configuration_descriptor();
  SmartBuffer response(sizeof(configuration.bConfigurationValue));
  response.Add(&configuration.bConfigurationValue,
               sizeof(configuration.bConfigurationValue));
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::HandleUnsupportedRequest(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleUnsupportedRequest %u: %u[%u]\n", control_request.bRequest,
         control_request.wValue1, control_request.wValue0);
  SendUsbControlResponse(sockfd, usb_request, 0, 0);
}

void UsbPrinter::HandleGetDeviceId(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const UsbControlRequest& control_request) const {
  printf("HandleGetDeviceId %u[%u]\n", control_request.wValue1,
         control_request.wValue0);

  SmartBuffer response(ieee_device_id().size());
  response.Add(ieee_device_id());
  SendUsbControlResponse(sockfd, usb_request, response.data(), response.size());
}

void UsbPrinter::WriteDocument(const SmartBuffer& data) const {
  if (!record_document_file_.IsValid()) {
    LOG(ERROR) << "File is invalid: " << record_document_path_;
    LOG(ERROR) << "Error code: " << FileError();
    exit(1);
  }
  int wrote = record_document_file_.Write(
      0, reinterpret_cast<const char*>(data.data()), data.size());
  if (wrote != data.size()) {
    LOG(ERROR) << "Failed to write document to file: " << record_document_path_;
  }
}

void UsbPrinter::HandleGetPrinterAttributes(
    int sockfd, const UsbipCmdSubmit& usb_request,
    const IppHeader& request_header) const {
  printf("HandleGetPrinterAttributes %u\n", request_header.request_id);

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(printer_attributes_) + 1;
  SmartBuffer buf(response_size);
  AddIppHeader(response_header, &buf);
  LOG(INFO) << "Adding operation attributes";
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  LOG(INFO) << "Adding printer attributes";
  AddPrinterAttributes(printer_attributes_, kPrinterAttributes, &buf);
  AddEndOfAttributes(&buf);
  QueueIppUsbResponse(usb_request, buf);
}

void UsbPrinter::HandleGetJobAttributes(const UsbipCmdSubmit& usb_request,
                                        const IppHeader& request_header) const {
 IppHeader response_header = request_header;
 response_header.operation_id = kSuccessStatus;
 // We add 1 to the size for the end of attributes tag.
 size_t response_size = sizeof(response_header) +
                        GetAttributesSize(operation_attributes_) +
                        GetAttributesSize(job_attributes_) + 1;
 SmartBuffer buf(response_size);
 AddIppHeader(response_header, & buf);
 AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
 AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
 AddEndOfAttributes(&buf);
 QueueIppUsbResponse(usb_request, buf);
}

void UsbPrinter::QueueIppUsbResponse(
    const UsbipCmdSubmit& usb_request,
    const SmartBuffer& attributes_buffer) const {
  const std::string http_header =
      GetHttpResponseHeader(attributes_buffer.size());
  SmartBuffer http_message(http_header.size());
  http_message.Add(http_header.c_str(), http_header.size());
  http_message.Add(attributes_buffer);

  LOG(INFO) << "Queueing ipp response...";
  QueueMessage(usb_request.header.ep, http_message);
}

void UsbPrinter::HandleValidateJob(const UsbipCmdSubmit& usb_request,
                                   const IppHeader& request_header) const {
  printf("HandleValidateJob %u\n", request_header.request_id);

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size =
      sizeof(response_header) + GetAttributesSize(operation_attributes_) + 1;
  SmartBuffer buf(response_size);
  AddIppHeader(response_header, &buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddEndOfAttributes(&buf);
  QueueIppUsbResponse(usb_request, buf);
}

void UsbPrinter::HandleCreateJob(const UsbipCmdSubmit& usb_request,
                                 const IppHeader& request_header) const {
  printf("HandleCreateJob %u\n", request_header.request_id);

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  AddIppHeader(response_header, &buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  QueueIppUsbResponse(usb_request, buf);
}

void UsbPrinter::HandleSendDocument(const UsbipCmdSubmit& usb_request,
                                    const IppHeader& request_header) const {
  printf("HandleSendDocument %u\n", request_header.request_id);

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  AddIppHeader(response_header, &buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  QueueIppUsbResponse(usb_request, buf);
}

void UsbPrinter::HandleBulkInRequest(int sockfd,
                                     const UsbipCmdSubmit& usb_request) const {
  if (QueueEmpty(usb_request.header.ep)) {
    SendUsbControlResponse(sockfd, usb_request, 0, 0);
    return;
  }
  LOG(INFO) << "Responding with queued message...";
  SmartBuffer http_message = PopMessage(usb_request.header.ep);

  size_t max_size = usb_request.transfer_buffer_length;

  UsbipRetSubmit response = CreateUsbipRetSubmit(usb_request);
  response.header.direction = 1;
  response.actual_length = std::min(max_size, http_message.size());
  size_t response_size = sizeof(response);
  PackUsbip(reinterpret_cast<int*>(&response), response_size);
  SmartBuffer response_buffer(response_size);
  response_buffer.Add(&response, response_size);

  if (http_message.size() > max_size) {
    size_t leftover_size = http_message.size() - max_size;
    SmartBuffer leftover(leftover_size);
    leftover.Add(http_message, max_size);
    http_message.Shrink(max_size);
    LOG(INFO) << "Queueing leftover message...";
    QueueMessage(usb_request.header.ep, leftover);
  }
  response_buffer.Add(http_message);
  SendBuffer(sockfd, response_buffer);
}
