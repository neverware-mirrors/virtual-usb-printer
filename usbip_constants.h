// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __USBIP_CONSTANTS_H__
#define __USBIP_CONSTANTS_H__

// USB Decriptor Type Constants.
#define USB_DESCRIPTOR_DEVICE 0x01            // Device Descriptor.
#define USB_DESCRIPTOR_CONFIGURATION 0x02     // Configuration Descriptor.
#define USB_DESCRIPTOR_STRING 0x03            // String Descriptor.
#define USB_DESCRIPTOR_INTERFACE 0x04         // Interface Descriptor.
#define USB_DESCRIPTOR_ENDPOINT 0x05          // Endpoint Descriptor.
#define USB_DESCRIPTOR_DEVICE_QUALIFIER 0x06  // Device Qualifier.

#define STANDARD_TYPE 0  // Standard USB Request.
#define CLASS_TYPE 1     // Class-specific USB Request.
#define VENDOR_TYPE 2    // Vendor-specific USB Request.
#define RESERVED_TYPE 3  // Reserved.

// USB "bRequest" Constants.
// These represent the possible values contained within a USB SETUP packet which
// specify the type of request.
#define GET_STATUS 0x00
#define CLEAR_FEATURE 0x01
#define SET_FEATURE 0x03
#define SET_ADDRESS 0x05
#define GET_DESCRIPTOR 0x06
#define SET_DESCRIPTOR 0x07
#define GET_CONFIGURATION 0x08
#define SET_CONFIGURATION 0x09
#define GET_INTERFACE 0x0A
#define SET_INTERFACE 0x0B
#define SET_FRAME 0x0C

// Special "bRequest" values for printer requests.
#define GET_DEVICE_ID 0
#define GET_PORT_STATUS 1
#define SOFT_RESET 2

// OP Commands.
#define OP_REQ_IMPORT_CMD 0x8003
#define OP_REP_IMPORT_CMD 0x0003
#define OP_REQ_DEVLIST_CMD 0x8005
#define OP_REP_DEVLIST_CMD 0x0005

// USBIP Command Constants.
#define COMMAND_USBIP_CMD_SUBMIT 0x0001
#define COMMAND_USBIP_CMD_UNLINK 0X0002
#define COMMAND_USBIP_RET_SUBMIT 0x0003
#define COMMAND_USBIP_RET_UNLINK 0x0004

// IPP Operation IDs.
#define IPP_VALIDATE_JOB 0x0004
#define IPP_CREATE_JOB 0x0005
#define IPP_SEND_DOCUMENT 0x0006
#define IPP_GET_JOB_ATTRIBUTES 0x0009
#define IPP_GET_PRINTER_ATTRIBUTES 0x000B

// Port that the server is bound to.
#define TCP_SERV_PORT 3240

#endif  // __USBIP_CONSTANTS_H__
