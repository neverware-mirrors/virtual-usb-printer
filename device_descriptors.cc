// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_descriptors.h"

#include <cstdio>

bool operator==(const UsbDeviceDescriptor& lhs,
                const UsbDeviceDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bcdUSB == rhs.bcdUSB && lhs.bDeviceClass == rhs.bDeviceClass &&
         lhs.bDeviceSubClass == rhs.bDeviceSubClass &&
         lhs.bDeviceProtocol == rhs.bDeviceProtocol &&
         lhs.bMaxPacketSize0 == rhs.bMaxPacketSize0 &&
         lhs.idVendor == rhs.idVendor && lhs.idProduct == rhs.idProduct &&
         lhs.bcdDevice == rhs.bcdDevice &&
         lhs.iManufacturer == rhs.iManufacturer &&
         lhs.iProduct == rhs.iProduct &&
         lhs.iSerialNumber == rhs.iSerialNumber &&
         lhs.bNumConfigurations == rhs.bNumConfigurations;
}

bool operator==(const UsbConfigurationDescriptor& lhs,
                const UsbConfigurationDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.wTotalLength == rhs.wTotalLength &&
         lhs.bNumInterfaces == rhs.bNumInterfaces &&
         lhs.bConfigurationValue == rhs.bConfigurationValue &&
         lhs.iConfiguration == rhs.iConfiguration &&
         lhs.bmAttributes == rhs.bmAttributes && lhs.bMaxPower == rhs.bMaxPower;
}

bool operator==(const UsbInterfaceDescriptor& lhs,
                const UsbInterfaceDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bInterfaceNumber == rhs.bInterfaceNumber &&
         lhs.bAlternateSetting == rhs.bAlternateSetting &&
         lhs.bNumEndpoints == rhs.bNumEndpoints &&
         lhs.bInterfaceClass == rhs.bInterfaceClass &&
         lhs.bInterfaceSubClass == rhs.bInterfaceSubClass &&
         lhs.bInterfaceProtocol == rhs.bInterfaceProtocol &&
         lhs.iInterface == rhs.iInterface;
}

bool operator==(const UsbEndpointDescriptor& lhs,
                const UsbEndpointDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bEndpointAddress == rhs.bEndpointAddress &&
         lhs.bmAttributes == rhs.bmAttributes &&
         lhs.wMaxPacketSize == rhs.wMaxPacketSize &&
         lhs.bInterval == rhs.bInterval;
}

bool operator==(const UsbDeviceQualifierDescriptor& lhs,
                const UsbDeviceQualifierDescriptor& rhs) {
  return lhs.bLength == rhs.bLength &&
         lhs.bDescriptorType == rhs.bDescriptorType &&
         lhs.bcdUSB == rhs.bcdUSB && lhs.bDeviceClass == rhs.bDeviceClass &&
         lhs.bDeviceSubClass == rhs.bDeviceSubClass &&
         lhs.bDeviceProtocol == rhs.bDeviceProtocol &&
         lhs.bMaxPacketSize0 == rhs.bMaxPacketSize0 &&
         lhs.bNumConfigurations == rhs.bNumConfigurations &&
         lhs.bReserved == rhs.bReserved;
}

void PrintDeviceDescriptor(const UsbDeviceDescriptor& dev) {
  printf("Device Descriptor\n");
  printf("\tbLength: %u\n", (unsigned int)dev.bLength);
  printf("\tbDescriptorType: %u\n", (unsigned int)dev.bDescriptorType);
  printf("\tbcdUSB: %u\n", (unsigned int)dev.bcdUSB);
  printf("\tbDeviceClass: %u\n", (unsigned int)dev.bDeviceClass);
  printf("\tbDeviceSubClass: %u\n", (unsigned int)dev.bDeviceSubClass);
  printf("\tbDeviceProtocol: %u\n", (unsigned int)dev.bDeviceProtocol);
  printf("\tbMaxPacketSize0: %u\n", (unsigned int)dev.bMaxPacketSize0);
  printf("\tidVendor: %04x\n", (unsigned int)dev.idVendor);
  printf("\tidProduct: %04x\n", (unsigned int)dev.idProduct);
  printf("\tbcdDevice: %u\n", (unsigned int)dev.bcdDevice);
  printf("\tiManufacturer: %u\n", (unsigned int)dev.iManufacturer);
  printf("\tiProduct: %u\n", (unsigned int)dev.iProduct);
  printf("\tiSerialNumber: %u\n", (unsigned int)dev.iSerialNumber);
  printf("\tbNumConfigurations: %u\n", (unsigned int)dev.bNumConfigurations);
}

void PrintConfigurationDescriptor(const UsbConfigurationDescriptor& conf) {
  printf("Configuration Descriptor\n");
  printf("\tbLength: %u\n", (unsigned int)conf.bLength);
  printf("\tbDescriptorType: %u\n", (unsigned int)conf.bDescriptorType);
  printf("\twTotalLength: %u\n", (unsigned int)conf.wTotalLength);
  printf("\tbNumInterfaces: %u\n", (unsigned int)conf.bNumInterfaces);
  printf("\tbConfigurationValue: %u\n", (unsigned int)conf.bConfigurationValue);
  printf("\tiConfiguration: %u\n", (unsigned int)conf.iConfiguration);
  printf("\tbmAttributes: %u\n", (unsigned int)conf.bmAttributes);
  printf("\tbMaxPower: %u\n", (unsigned int)conf.bMaxPower);
}

void PrintInterfaceDescriptor(const UsbInterfaceDescriptor& intf) {
  printf("Interface Descriptor\n");
  printf("\tbLength: %u\n", (unsigned int)intf.bLength);
  printf("\tbDescriptorType: %u\n", (unsigned int)intf.bDescriptorType);
  printf("\tbInterfaceNumber: %u\n", (unsigned int)intf.bInterfaceNumber);
  printf("\tbAlternateSetting: %u\n", (unsigned int)intf.bAlternateSetting);
  printf("\tbNumEndpoints: %u\n", (unsigned int)intf.bNumEndpoints);
  printf("\tbInterfaceClass: %u\n", (unsigned int)intf.bInterfaceClass);
  printf("\tbInterfaceSubClass: %u\n", (unsigned int)intf.bInterfaceSubClass);
  printf("\tbInterfaceProtocol: %u\n", (unsigned int)intf.bInterfaceProtocol);
  printf("\tiInterface: %u\n", (unsigned int)intf.iInterface);
}

void PrintEndpointDescriptor(const UsbEndpointDescriptor& endp) {
  printf("Endpoint Descriptor\n");
  printf("\tbLength: %u\n", (unsigned int)endp.bLength);
  printf("\tbDescriptorType: %u\n", (unsigned int)endp.bDescriptorType);
  printf("\tbEndpointAddress: %#04x\n", (unsigned int)endp.bEndpointAddress);
  printf("\tbmAttributes: %u\n", (unsigned int)endp.bmAttributes);
  printf("\twMaxPacketSize: %u\n", (unsigned int)endp.wMaxPacketSize);
  printf("\tbInterval: %u\n", (unsigned int)endp.bInterval);
}
