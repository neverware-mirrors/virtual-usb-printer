#include "device_descriptors.h"

#include <cstdio>

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
