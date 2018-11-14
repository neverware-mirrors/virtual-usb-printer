// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_USB_PRINTER_H__
#define __USBIP_USB_PRINTER_H__

#include "device_descriptors.h"
#include "ipp_util.h"
#include "smart_buffer.h"
#include "usbip.h"
#include "usbip_constants.h"

#include <map>
#include <queue>
#include <vector>

#include <base/files/file.h>

// This class is responsible for managing an ippusb interface of a printer. It
// keeps track of whether or not the interface is currently in the process of
// receiving a chunked IPP message, and queues up responses to IPP requests so
// that they can be sent when a BULK IN request is received.
class InterfaceManager {
 public:
  InterfaceManager() = default;

  // Place the IPP response |message| on the end of |queue_|.
  void QueueMessage(const SmartBuffer& message);

  // Returns whether or not |queue_| is empty.
  bool QueueEmpty() const;

  // Returns the message at the front of |queue_| and removes it. If PopMessage
  // is called when |queue_| is empty then the program will exit.
  SmartBuffer PopMessage();

  bool receiving_chunked() const { return receiving_chunked_; };
  void SetReceivingChunked(bool b) { receiving_chunked_ = b; }

  const IppHeader& chunked_ipp_header() const { return chunked_ipp_header_; }
  void SetChunkedIppHeader(const IppHeader& ipp_header) {
    chunked_ipp_header_ = ipp_header;
  }

  SmartBuffer* chunked_message() {
    return &chunked_message_;
  }
  void ChunkedMessageAdd(const SmartBuffer& message);

  const SmartBuffer& document() const { return document_; }
  void SetDocument(const SmartBuffer& document) { document_ = document; }

  const SmartBuffer& ipp_message() const { return ipp_message_; }
  void SetIppMessage(const SmartBuffer& ipp_message) {
    ipp_message_ = ipp_message;
  }

 private:
  std::queue<SmartBuffer> queue_;
  // Represents whether the interface is currently receiving an HTTP "chunked"
  // message.
  bool receiving_chunked_;
  IppHeader chunked_ipp_header_;
  SmartBuffer chunked_message_;

  SmartBuffer ipp_message_;
  SmartBuffer document_;
};

// A grouping of the descriptors for a USB device.
class UsbDescriptors {
 public:
  explicit UsbDescriptors(
      const UsbDeviceDescriptor& device_descriptor,
      const UsbConfigurationDescriptor& configuration_descriptor,
      const UsbDeviceQualifierDescriptor& qualifier_descriptor,
      const std::vector<std::vector<char>>& string_descriptors,
      const std::vector<char>& ieee_device_id,
      const std::vector<UsbInterfaceDescriptor>& interfaces,
      const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>& endpoints);

  const UsbDeviceDescriptor& device_descriptor() const {
    return device_descriptor_;
  }

  const UsbConfigurationDescriptor& configuration_descriptor() const {
    return configuration_descriptor_;
  }

  const UsbDeviceQualifierDescriptor& qualifier_descriptor() const {
    return qualifier_descriptor_;
  }

  const std::vector<std::vector<char>>& string_descriptors() const {
    return string_descriptors_;
  }

  const std::vector<char>& ieee_device_id() const { return ieee_device_id_; }

  const std::vector<UsbInterfaceDescriptor>& interface_descriptors() const {
    return interface_descriptors_;
  }

  const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>&
  endpoint_descriptors() const {
    return endpoint_descriptors_;
  }

 private:
  UsbDeviceDescriptor device_descriptor_;
  UsbConfigurationDescriptor configuration_descriptor_;
  UsbDeviceQualifierDescriptor qualifier_descriptor_;

  // Represents the strings attributes of the printer.
  // Since these strings may contain '0' bytes, we can't use std::string to
  // represent them since they would interpret them as an end-of-string
  // character.
  // For more information about strings descriptors refer to: Section 9.6
  // Standard USB Descriptor Definitions
  // https://www.usb.org/document-library/usb-20-specification-released-april-27-2000
  std::vector<std::vector<char>> string_descriptors_;

  // As with USB string descriptors the IEEE device id may contain a 0 byte in
  // the portion which indicates the message size, so a vector is used instead
  // of a string.
  std::vector<char> ieee_device_id_;
  std::vector<UsbInterfaceDescriptor> interface_descriptors_;

  // Maps interface numbers to their respective collection of endpoint
  // descriptors.
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoint_descriptors_;
};

// Represents a single USB printer and can respond to basic USB control requests
// and printer-specific USB requests.
class UsbPrinter {
 public:
  explicit UsbPrinter(const UsbDescriptors& usb_descriptors);

  const UsbDeviceDescriptor& device_descriptor() const {
    return usb_descriptors_.device_descriptor();
  }

  const UsbConfigurationDescriptor& configuration_descriptor() const {
    return usb_descriptors_.configuration_descriptor();
  }

  const UsbDeviceQualifierDescriptor& qualifier_descriptor() const {
    return usb_descriptors_.qualifier_descriptor();
  }

  const std::vector<std::vector<char>>& string_descriptors() const {
    return usb_descriptors_.string_descriptors();
  }

  const std::vector<char>& ieee_device_id() const {
    return usb_descriptors_.ieee_device_id();
  }

  const std::vector<UsbInterfaceDescriptor>& interface_descriptors() const {
    return usb_descriptors_.interface_descriptors();
  }

  const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>&
  endpoint_descriptors() const {
    return usb_descriptors_.endpoint_descriptors();
  }

  std::string record_document_path() const { return record_document_path_; }

  // Returns true if this printer supports ipp-over-usb. An ippusb printer must
  // have at least 2 interfaces with the following values:
  //    bInterfaceClass: 7
  //    bInterfaceSubClass: 1
  //    bInterfaceProtocol: 4
  bool IsIppUsb() const;

  std::vector<IppAttribute> operation_attributes() const {
    return operation_attributes_;
  }

  std::vector<IppAttribute> printer_attributes() const {
    return printer_attributes_;
  }

  void SetOperationAttributes(
      const std::vector<IppAttribute>& operation_attributes) {
    operation_attributes_ = operation_attributes;
  }

  void SetPrinterAttributes(
      const std::vector<IppAttribute>& printer_attributes) {
    printer_attributes_ = printer_attributes;
  }

  void SetJobAttributes(const std::vector<IppAttribute>& job_attributes) {
    job_attributes_ = job_attributes;
  }

  // TODO(valleau): Look into making these attributes dynamic as we should only
  // report unsupported attributes if they were requested by the client.
  void SetUnsupportedAttributes(
      const std::vector<IppAttribute>& unsupported_attributes) {
    unsupported_attributes_ = unsupported_attributes;
  }

  // Determines whether |usb_request| is either a control or data request and
  // defers to the corresponding function.
  void HandleUsbRequest(int sockd, const UsbipCmdSubmit& usb_request) const;

  // Determines whether |usb_request| is either a standard or class-specific
  // control request and defers to the corresponding function.
  void HandleUsbControl(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleUsbData(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleIppUsbData(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleIppUsbData(int sockfd, const UsbipCmdSubmit& usb_request,
                        const IppHeader& ipp_header) const;

  void HandleChunkedIppUsbData(int sockfd, const UsbipCmdSubmit& usb_request,
                               SmartBuffer* message) const;

  // Handles the standard USB requests.
  void HandleStandardControl(int sockfd, const UsbipCmdSubmit& usb_request,
                             const UsbControlRequest& control_request) const;

  // Handles printer-specific USB requests.
  void HandlePrinterControl(int sockfd, const UsbipCmdSubmit& usb_request,
                            const UsbControlRequest& control_request) const;

  // Sets |path| as the as the location to store documents received from print
  // jobs. Sets |record_document_| to true to indicate that documents received
  // in print jobs should now be recorded. Returns true if the creation of the
  // file at |path| was successful.
  bool SetupRecordDocument(const std::string& path);

  // Returns the error from |record_document_file_|. Used to identify why file
  // creation failed.
  base::File::Error FileError() const;

  // Enqueue |message| on the interface specified by |ep|.
  void QueueMessage(int ep, const SmartBuffer& message) const;

  // Pop the IPP response message from the interface specified by |ep|.
  SmartBuffer PopMessage(int ep) const;

  // Returns whether the response queue in the interface specified by |ep| is
  // empty.
  bool QueueEmpty(int ep) const;

  // Marks whether the InterfaceManager corresponding to endpoint number |ep| is
  // currently receiving a chunked HTTP message using |b|.
  void SetReceivingChunked(int ep, bool b) const;

  // Returns whether the interface specified by |ep| is currently receiving a
  // chunked IPP message.
  bool GetReceivingChunked(int ep) const;

  void SetChunkedIppHeader(int ep, const IppHeader& ipp_header) const;
  const IppHeader& GetChunkedIppHeader(int ep) const;

  void ChunkedMessageAdd(int ep, const SmartBuffer& message) const;
  SmartBuffer* GetChunkedMessage(int ep) const;

  void SetIppMessage(int ep, const SmartBuffer& ipp_message) const;
  const SmartBuffer& GetIppMessage(int ep) const;

  void SetDocument(int ep, const SmartBuffer& document) const;
  const SmartBuffer& GetDocument(int ep) const;

 private:
  void HandleGetStatus(int sockfd, const UsbipCmdSubmit& usb_request,
                       const UsbControlRequest& control_request) const;

  void HandleGetDescriptor(int sockfd, const UsbipCmdSubmit& usb_request,
                           const UsbControlRequest& control_request) const;

  void HandleGetDeviceDescriptor(
      int sockfd, const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;

  void HandleGetConfigurationDescriptor(
      int sockfd, const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;

  void HandleGetDeviceQualifierDescriptor(
      int sockfd, const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;

  void HandleGetStringDescriptor(
      int sockfd, const UsbipCmdSubmit& usb_request,
      const UsbControlRequest& control_request) const;

  void HandleGetConfiguration(int sockfd, const UsbipCmdSubmit& usb_request,
                              const UsbControlRequest& control_request) const;

  void HandleSetConfiguration(int sockfd, const UsbipCmdSubmit& usb_request,
                              const UsbControlRequest& control_request) const;

  void HandleSetInterface(int sockfd, const UsbipCmdSubmit& usb_request,
                          const UsbControlRequest& control_request) const;

  void HandleGetDeviceId(int sockfd, const UsbipCmdSubmit& usb_request,
                         const UsbControlRequest& control_request) const;

  // Write the document contained within |data| to |record_document_file_|.
  void WriteDocument(const SmartBuffer& data) const;

  void HandleGetPrinterAttributes(int sockfd, const UsbipCmdSubmit& usb_request,
                                  const IppHeader& ipp_header) const;

  void HandleValidateJob(const UsbipCmdSubmit& usb_request,
                         const IppHeader& request_header) const;

  void HandleCreateJob(const UsbipCmdSubmit& usb_request,
                       const IppHeader& request_header) const;

  void HandleSendDocument(const UsbipCmdSubmit& usb_request,
                          const IppHeader& request_header) const;

  void HandleGetJobAttributes(const UsbipCmdSubmit& usb_request,
                              const IppHeader& request_header) const;

  void QueueIppUsbResponse(const UsbipCmdSubmit& usb_request,
                           const SmartBuffer& attributes_buffer) const;

  // Responds to a BULK_IN request by replying with the message at the front of
  // |message_queue_|.
  void HandleBulkInRequest(int sockfd, const UsbipCmdSubmit& usb_request) const;

  // Determines the index for the InterfaceManager that corresponds to the given
  // endpoint number |ep|.
  int GetInterfaceManagerIndex(int ep) const;

  UsbDescriptors usb_descriptors_;

  bool record_document_;
  std::string record_document_path_;
  mutable base::File record_document_file_;

  std::vector<IppAttribute> operation_attributes_;
  std::vector<IppAttribute> printer_attributes_;
  std::vector<IppAttribute> job_attributes_;
  std::vector<IppAttribute> unsupported_attributes_;

  mutable std::vector<InterfaceManager> interface_managers_;
};

#endif  // __USBIP_USB_PRINTER_H__
