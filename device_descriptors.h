// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_DESCRIPTORS_H__
#define DEVICE_DESCRIPTORS_H__

#include <cstdint>

#include "usbip_constants.h"

// USB Descriptors implemented according to:
// Universal Serial Bus Specification Revision 2.0
// Section 9.6 Standard USB Descriptor Definitions
// https://www.usb.org/document-library/usb-20-specification-released-april-27-2000

// USB Device Descriptor
struct UsbDeviceDescriptor {
  UsbDeviceDescriptor() = default;
  UsbDeviceDescriptor(uint8_t _bLength, uint8_t _bDescriptorType,
                      uint16_t _bcdUSB, uint8_t _bDeviceClass,
                      uint8_t _bDeviceSubClass, uint8_t _bDeviceProtocol,
                      uint8_t _bMaxPacketSize0, uint16_t _idVendor,
                      uint16_t _idProduct, uint16_t _bcdDevice,
                      uint8_t _iManufacturer, uint8_t _iProduct,
                      uint8_t _iSerialNumber, uint8_t _bNumConfigurations)
      : bLength(_bLength),
        bDescriptorType(_bDescriptorType),
        bcdUSB(_bcdUSB),
        bDeviceClass(_bDeviceClass),
        bDeviceSubClass(_bDeviceSubClass),
        bDeviceProtocol(_bDeviceProtocol),
        bMaxPacketSize0(_bMaxPacketSize0),
        idVendor(_idVendor),
        idProduct(_idProduct),
        bcdDevice(_bcdDevice),
        iManufacturer(_iManufacturer),
        iProduct(_iProduct),
        iSerialNumber(_iSerialNumber),
        bNumConfigurations(_bNumConfigurations) {}

  uint8_t bLength;             // Size of this descriptor in uint8_ts.
  uint8_t bDescriptorType;     // Type = 0x01 (USB_DESCRIPTOR_DEVICE).
  uint16_t bcdUSB;             // USB Spec Release number in BCD format.
  uint8_t bDeviceClass;        // Class code.
  uint8_t bDeviceSubClass;     // Subclass code.
  uint8_t bDeviceProtocol;     // Protocol code.
  uint8_t bMaxPacketSize0;     // Max packet size of endpoint 0.
  uint16_t idVendor;           // Vendor ID.
  uint16_t idProduct;          // Product ID.
  uint16_t bcdDevice;          // Device release number in BCD format.
  uint8_t iManufacturer;       // Index of manufacturer string descriptor.
  uint8_t iProduct;            // Index of product string descriptor.
  uint8_t iSerialNumber;       // Index of serial number string descriptor.
  uint8_t bNumConfigurations;  // Number of possible configurations.
} __attribute__((packed));

// USB Configuration Descriptor
struct UsbConfigurationDescriptor {
  UsbConfigurationDescriptor() = default;
  UsbConfigurationDescriptor(uint8_t _bLength, uint8_t _bDescriptorType,
                             uint16_t _wTotalLength, uint8_t _bNumInterfaces,
                             uint8_t _bConfigurationValue,
                             uint8_t _iConfiguration, uint8_t _bmAttributes,
                             uint8_t _bMaxPower)
      : bLength(_bLength),
        bDescriptorType(_bDescriptorType),
        wTotalLength(_wTotalLength),
        bNumInterfaces(_bNumInterfaces),
        bConfigurationValue(_bConfigurationValue),
        iConfiguration(_iConfiguration),
        bmAttributes(_bmAttributes),
        bMaxPower(_bMaxPower) {}

  uint8_t bLength;          // Size of this descriptor in uint8_ts.
  uint8_t bDescriptorType;  // Type = 0x02 (USB_DESCRIPTOR_CONFIGURATION).
  uint16_t wTotalLength;    // Length of configuration including interface and
                            // endpoint descriptors.
  uint8_t bNumInterfaces;   // Number of interfaces in the configuration.
  uint8_t bConfigurationValue;  // Index value for this configuration.
  uint8_t iConfiguration;       // Index of configuration string descriptor.
  uint8_t bmAttributes;  // Bitmap containing configuration characteristics.
  uint8_t bMaxPower;     // Max power consumption (2x mA).
} __attribute__((packed));

// USB Interface Descriptor
struct UsbInterfaceDescriptor {
  UsbInterfaceDescriptor() = default;
  UsbInterfaceDescriptor(uint8_t _bLength, uint8_t _bDescriptorType,
                         uint8_t _bInterfaceNumber, uint8_t _bAlternateSetting,
                         uint8_t _bNumEndpoints, uint8_t _bInterfaceClass,
                         uint8_t _bInterfaceSubClass,
                         uint8_t _bInterfaceProtocol, uint8_t _iInterface)
      : bLength(_bLength),
        bDescriptorType(_bDescriptorType),
        bInterfaceNumber(_bInterfaceNumber),
        bAlternateSetting(_bAlternateSetting),
        bNumEndpoints(_bNumEndpoints),
        bInterfaceClass(_bInterfaceClass),
        bInterfaceSubClass(_bInterfaceSubClass),
        bInterfaceProtocol(_bInterfaceProtocol),
        iInterface(_iInterface) {}

  uint8_t bLength;             // Size of this descriptor in uint8_ts.
  uint8_t bDescriptorType;     // Type = 0x04 (USB_DESCRIPTOR_INTERFACE).
  uint8_t bInterfaceNumber;    // Index of this interface.
  uint8_t bAlternateSetting;   // Alternate setting number.
  uint8_t bNumEndpoints;       // Number of endpoints in this interface.
  uint8_t bInterfaceClass;     // Class code.
  uint8_t bInterfaceSubClass;  // Subclass code.
  uint8_t bInterfaceProtocol;  // Protocol code.
  uint8_t iInterface;          // Index of string descriptor for this interface.
} __attribute__((packed));

// USB Endpoint Descriptor
struct UsbEndpointDescriptor {
  UsbEndpointDescriptor() = default;
  UsbEndpointDescriptor(uint8_t _bLength, uint8_t _bDescriptorType,
                        uint8_t _bEndpointAddress, uint8_t _bmAttributes,
                        uint16_t _wMaxPacketSize, uint8_t _bInterval)
      : bLength(_bLength),
        bDescriptorType(_bDescriptorType),
        bEndpointAddress(_bEndpointAddress),
        bmAttributes(_bmAttributes),
        wMaxPacketSize(_wMaxPacketSize),
        bInterval(_bInterval) {}

  uint8_t bLength;           // Size of this descriptor in uint8_ts.
  uint8_t bDescriptorType;   // Type = 0x05 (USB_DESCRIPTOR_ENDPOINT).
  uint8_t bEndpointAddress;  // Endpoint address.
  uint8_t bmAttributes;      // Attributes of the endpoint.
  uint16_t wMaxPacketSize;   // The maximum packet size for this endpoint.
  uint8_t bInterval;  // Interval for polling endpoint for data transfers.
} __attribute__((packed));

// USB Device Qualifier Descriptor
struct UsbDeviceQualifierDescriptor {
  UsbDeviceQualifierDescriptor() = default;
  UsbDeviceQualifierDescriptor(uint8_t _bLength, uint8_t _bDescriptorType,
                               uint16_t _bcdUSB, uint8_t _bDeviceClass,
                               uint8_t _bDeviceSubClass,
                               uint8_t _bDeviceProtocol,
                               uint8_t _bMaxPacketSize0,
                               uint8_t _bNumConfigurations, uint8_t _bReserved)
      : bLength(_bLength),
        bDescriptorType(_bDescriptorType),
        bcdUSB(_bcdUSB),
        bDeviceClass(_bDeviceClass),
        bDeviceSubClass(_bDeviceSubClass),
        bDeviceProtocol(_bDeviceProtocol),
        bMaxPacketSize0(_bMaxPacketSize0),
        bNumConfigurations(_bNumConfigurations),
        bReserved(_bReserved) {}

  uint8_t bLength;             // Size of this descriptor in uint8_ts.
  uint8_t bDescriptorType;     // Type = 0x06 (USB_DESCRIPTOR_DEVICE_QUALIFIER).
  uint16_t bcdUSB;             // USB Spec release number in BCD format.
  uint8_t bDeviceClass;        // Class code.
  uint8_t bDeviceSubClass;     // Subclass code.
  uint8_t bDeviceProtocol;     // Protocol code.
  uint8_t bMaxPacketSize0;     // Max packet size for other speed.
  uint8_t bNumConfigurations;  // Number of other speed configurations.
  uint8_t bReserved;           // Reserved, must be zero (0)
} __attribute__((packed));

bool operator==(const UsbDeviceDescriptor& lhs, const UsbDeviceDescriptor& rhs);
bool operator==(const UsbConfigurationDescriptor& lhs,
                const UsbConfigurationDescriptor& rhs);
bool operator==(const UsbInterfaceDescriptor& lhs,
                const UsbInterfaceDescriptor& rhs);
bool operator==(const UsbEndpointDescriptor& lhs,
                const UsbEndpointDescriptor& rhs);
bool operator==(const UsbDeviceQualifierDescriptor& lhs,
                const UsbDeviceQualifierDescriptor& rhs);

void PrintDeviceDescriptor(const UsbDeviceDescriptor& dev);
void PrintConfigurationDescriptor(const UsbConfigurationDescriptor& conf);
void PrintInterfaceDescriptor(const UsbInterfaceDescriptor& intf);
void PrintEndpointDescriptor(const UsbEndpointDescriptor& endp);

#endif  // DEVICE_DESCRIPTORS_H__
