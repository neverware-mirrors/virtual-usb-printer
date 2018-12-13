// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_USB_PRINTER_H__
#define __USBIP_USB_PRINTER_H__

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip.h"
#include "usbip_constants.h"

#include <map>
#include <vector>

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

  // Determines whether |usb_request| is either a control or data request and
  // defers to the corresponding function.
  void HandleUsbRequest(int sockd, const UsbipCmdSubmit& usb_request) const;

  // Determines whether |usb_request| is either a standard or class-specific
  // control request and defers to the corresponding function.
  void HandleUsbControl(int sockfd, const UsbipCmdSubmit& usb_request) const;

  void HandleUsbData(int sockfd, const UsbipCmdSubmit& usb_request) const;

  // Handles the standard USB requests.
  void HandleStandardControl(int sockfd, const UsbipCmdSubmit& usb_request,
                             const UsbControlRequest& control_request) const;

  // Handles printer-specific USB requests.
  void HandlePrinterControl(int sockfd, const UsbipCmdSubmit& usb_request,
                            const UsbControlRequest& control_request) const;

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

  void HandleBulkInRequest(int sockfd, const UsbipCmdSubmit& usb_request) const;

  UsbDescriptors usb_descriptors_;
};

#endif  // __USBIP_USB_PRINTER_H__
