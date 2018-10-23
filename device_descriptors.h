#ifndef __DEVICE_DESCRIPTORS_H__
#define __DEVICE_DESCRIPTORS_H__

#include "usbip_constants.h"

// USB Descriptors implemented according to:
// Universal Serial Bus Specification Revision 2.0
// Section 9.6 Standard USB Descriptor Definitions
// https://www.usb.org/document-library/usb-20-specification-released-april-27-2000

// USB Device Descriptor
struct UsbDeviceDescriptor {
  byte bLength;             // Size of this descriptor in bytes.
  byte bDescriptorType;     // Type = 0x01 (USB_DESCRIPTOR_DEVICE).
  word bcdUSB;              // USB Spec Release number in BCD format.
  byte bDeviceClass;        // Class code.
  byte bDeviceSubClass;     // Subclass code.
  byte bDeviceProtocol;     // Protocol code.
  byte bMaxPacketSize0;     // Max packet size of endpoint 0.
  word idVendor;            // Vendor ID.
  word idProduct;           // Product ID.
  word bcdDevice;           // Device release number in BCD format.
  byte iManufacturer;       // Index of manufacturer string descriptor.
  byte iProduct;            // Index of product string descriptor.
  byte iSerialNumber;       // Index of serial number string descriptor.
  byte bNumConfigurations;  // Number of possible configurations.
};

// USB Configuration Descriptor
struct UsbConfigurationDescriptor {
  byte bLength;              // Size of this descriptor in bytes.
  byte bDescriptorType;      // Type = 0x02 (USB_DESCRIPTOR_CONFIGURATION).
  word wTotalLength;         // Length of configuration including interface and
                             // endpoint descriptors.
  byte bNumInterfaces;       // Number of interfaces in the configuration.
  byte bConfigurationValue;  // Index value for this configuration.
  byte iConfiguration;       // Index of configuration string descriptor.
  byte bmAttributes;         // Bitmap containing configuration characteristics.
  byte bMaxPower;            // Max power consumption (2x mA).
};

// USB Interface Descriptor
struct UsbInterfaceDescriptor {
  byte bLength;             // Size of this descriptor in bytes.
  byte bDescriptorType;     // Type = 0x04 (USB_DESCRIPTOR_INTERFACE).
  byte bInterfaceNumber;    // Index of this interface.
  byte bAlternateSetting;   // Alternate setting number.
  byte bNumEndpoints;       // Number of endpoints in this interface.
  byte bInterfaceClass;     // Class code.
  byte bInterfaceSubClass;  // Subclass code.
  byte bInterfaceProtocol;  // Protocol code.
  byte iInterface;          // Index of string descriptor for this interface.
};

// USB Endpoint Descriptor
struct UsbEndpointDescriptor {
  byte bLength;           // Size of this descriptor in bytes.
  byte bDescriptorType;   // Type = 0x05 (USB_DESCRIPTOR_ENDPOINT).
  byte bEndpointAddress;  // Endpoint address.
  byte bmAttributes;      // Attributes of the endpoint.
  word wMaxPacketSize;    // The maximum packet size for this endpoint.
  byte bInterval;         // Interval for polling endpoint for data transfers.
};

// USB Device Qualifier Descriptor
struct UsbDeviceQualifierDescriptor {
  byte bLength;             // Size of this descriptor in bytes.
  byte bDescriptorType;     // Type = 0x06 (USB_DESCRIPTOR_DEVICE_QUALIFIER).
  word bcdUSB;              // USB Spec release number in BCD format.
  byte bDeviceClass;        // Class code.
  byte bDeviceSubClass;     // Subclass code.
  byte bDeviceProtocol;     // Protocol code.
  byte bMaxPacketSize0;     // Max packet size for other speed.
  byte bNumConfigurations;  // Number of other speed configurations.
  byte bReserved;           // Reserved, must be zero (0)
};

void PrintDeviceDescriptor(const UsbDeviceDescriptor& dev);
void PrintConfigurationDescriptor(const UsbConfigurationDescriptor& conf);
void PrintInterfaceDescriptor(const UsbInterfaceDescriptor& intf);
void PrintEndpointDescriptor(const UsbEndpointDescriptor& endp);

#endif  // __DEVICE_DESCRIPTORS_H__
