// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_USB_PRINTER_H__
#define __USBIP_USB_PRINTER_H__

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip_constants.h"
#include "usbip.h"

#include <map>
#include <vector>

// Represents a single USB printer and can respond to basic USB control requests
// and printer-specific USB requests.
class UsbPrinter {
 public:
  explicit UsbPrinter(
      const UsbDeviceDescriptor& device_descriptor,
      const UsbConfigurationDescriptor& configuration_descriptor,
      const UsbDeviceQualifierDescriptor& qualifier_descriptor,
      const std::vector<std::vector<char>>& strings,
      const std::vector<char> ieee_device_id,
      const std::vector<UsbInterfaceDescriptor>& interfaces,
      const std::map<uint8_t, std::vector<UsbEndpointDescriptor>>& endpoints);

  UsbDeviceDescriptor device_descriptor() const { return device_descriptor_; }

  UsbConfigurationDescriptor configuration_descriptor() const {
    return configuration_descriptor_;
  }

  std::vector<std::vector<char>> strings() const { return strings_; }

  std::vector<UsbInterfaceDescriptor> interfaces() const {
    return interfaces_;
  }

  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoints() const {
    return endpoints_;
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
  std::vector<std::vector<char>> strings_;
  std::vector<char> ieee_device_id_;
  std::vector<UsbInterfaceDescriptor> interfaces_;
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoints_;
};

#endif  // __USBIP_USB_PRINTER_H__
