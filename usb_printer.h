// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_USB_PRINTER_H__
#define __USBIP_USB_PRINTER_H__

#include <map>
#include <queue>
#include <vector>
#include <string>

#include <base/files/file.h>
#include <base/files/file_path.h>

#include "device_descriptors.h"
#include "escl_manager.h"
#include "http_util.h"
#include "ipp_manager.h"
#include "smart_buffer.h"
#include "usbip.h"

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

  bool receiving_message() const { return receiving_message_; }
  void set_receiving_message(bool b) { receiving_message_ = b; }

  bool receiving_chunked() const { return receiving_chunked_; }
  void set_receiving_chunked(bool b) { receiving_chunked_ = b; }

  HttpRequest request_header() const { return request_header_; }
  void set_request_header(const HttpRequest& r) { request_header_ = r; }

  SmartBuffer* message() { return &message_; }

 private:
  std::queue<SmartBuffer> queue_;
  // Represents whether the interface is currently receiving an HTTP message.
  bool receiving_message_;
  // Represents whether the interface is currently receiving an HTTP "chunked"
  // message.
  bool receiving_chunked_;
  HttpRequest request_header_;
  SmartBuffer message_;
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
  UsbPrinter(const UsbDescriptors& usb_descriptors,
             const base::FilePath& document_output_path,
             IppManager ipp_manager,
             EsclManager escl_manager);

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

  // Determines whether |usb_request| is either a control or data request and
  // defers to the corresponding function.
  void HandleUsbRequest(int sockfd, const UsbipCmdSubmit& usb_request);

 private:
  // Returns true if this printer supports ipp-over-usb. An ippusb printer must
  // have at least 2 interfaces with the following values:
  //    bInterfaceClass: 7
  //    bInterfaceSubClass: 1
  //    bInterfaceProtocol: 4
  bool IsIppUsb() const;

  // Determines whether |usb_request| is either a standard or class-specific
  // control request and defers to the corresponding function.
  void HandleUsbControl(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleUsbData(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleIppUsbData(int sockfd, const UsbipCmdSubmit& usb_request);

  void HandleHttpData(const UsbipCmdSubmit& usb_request, SmartBuffer* message);

  // Handles the standard USB requests.
  void HandleStandardControl(int sockfd, const UsbipCmdSubmit& usb_request,
                             const UsbControlRequest& control_request) const;

  // Handles printer-specific USB requests.
  void HandlePrinterControl(int sockfd, const UsbipCmdSubmit& usb_request,
                            const UsbControlRequest& control_request) const;

  // Get a pointer to the InterfaceManager that manages |endpoint|.
  InterfaceManager* GetInterfaceManager(int endpoint);

  HttpResponse GenerateHttpResponse(const HttpRequest& request,
                                    SmartBuffer* body);

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

  // Used to send an empty response to control requests which are not supported.
  void HandleUnsupportedRequest(int sockfd, const UsbipCmdSubmit& usb_request,
                                const UsbControlRequest& control_request) const;

  void HandleGetDeviceId(int sockfd, const UsbipCmdSubmit& usb_request,
                         const UsbControlRequest& control_request) const;

  void QueueHttpResponse(const UsbipCmdSubmit& usb_request,
                         const HttpResponse& response);

  // Responds to a BULK_IN request by replying with the message at the front of
  // |message_queue_|.
  void HandleBulkInRequest(int sockfd, const UsbipCmdSubmit& usb_request);

  UsbDescriptors usb_descriptors_;
  base::FilePath document_output_path_;

  IppManager ipp_manager_;
  EsclManager escl_manager_;
  std::vector<InterfaceManager> interface_managers_;
};

#endif  // __USBIP_USB_PRINTER_H__
