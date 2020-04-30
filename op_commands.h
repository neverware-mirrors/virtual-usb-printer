// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __OP_COMMANDS_H__
#define __OP_COMMANDS_H__

/*
 * This file defines the supported OP command messages from the usbip userspace
 * protocol, as well as some utility functions for processing them.
 *
 * In the context of the defined messages:
 *   "Req" is used in messages that submit a request.
 *   "Rep" is used in messages which reply to a request.
 *
 * For more information about the usbip protocol refer to the following
 * documentation:
 * https://www.kernel.org/doc/Documentation/usb/usbip_protocol.txt
 * https://en.opensuse.org/SDB:USBIP
 */

#include <cstdlib>
#include <vector>

#include "device_descriptors.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

// Forward declaration of UsbPrinter.
class UsbPrinter;

// Contains the header values that are contained within all of the "OP" messages
// used by usbip.
struct OpHeader {
  uint16_t version;  // usbip version.
  uint16_t command;  // op command type.
  int status;        // op request status.
};

// Generic device descriptor used by OpRepDevlist and OpRepImport.
struct OpRepDevice {
  char usbPath[256];
  char busID[32];
  int busnum;
  int devnum;
  int speed;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t bDeviceClass;
  uint8_t bDeviceSubClass;
  uint8_t bDeviceProtocol;
  uint8_t bConfigurationValue;
  uint8_t bNumConfigurations;
  uint8_t bNumInterfaces;
};

// The OpReqDevlistMessage message contains the same information as OpHeader.
typedef OpHeader OpReqDevlist;

// The header used in an OpRepDevlist message, the only difference from
// OpHeader is that it contains |numExportedDevices|.
struct OpRepDevlistHeader {
  OpHeader header;
  int numExportedDevices;
};

// Basic interface descriptor used by OpRepDevlist.
struct OpRepDevlistInterface {
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t padding;
};

// Defines the OpRepDevlist used to respond to a OpReqDevlist message.
struct OpRepDevlist {
  OpRepDevlistHeader header;
  // Since our program is used to provide a virtual USB device, we only include
  // a single OpRepDevice in our response.
  OpRepDevice device;
  OpRepDevlistInterface* interfaces;
};

// Defines the OpReqImport request used to request a device for import.
struct OpReqImport {
  OpHeader header;
  char busID[32];
};

// Defins the OpRepImport response which indicates whether the requested device
// was successfully exported.
struct OpRepImport {
  OpHeader header;
  OpRepDevice device;
};

// Sets the corresponding members of |header| using the given values.
void SetOpHeader(uint16_t command, int status, OpHeader* header);

// Sets the corresponding members of |devlist_header| using the given values.
void SetOpRepDevlistHeader(uint16_t command, int status, int numExportedDevices,
                           OpRepDevlistHeader* header);

// Sets the members of |device| using the corresponding values in
// |dev_dsc| and |config|.
void SetOpRepDevice(const UsbDeviceDescriptor& dev_dsc,
                    const UsbConfigurationDescriptor& configuration,
                    OpRepDevice* device);

// Assigns the values from |interfaces| into |rep_interfaces|.
void SetOpRepDevlistInterfaces(
    const std::vector<UsbInterfaceDescriptor>& interfaces,
    OpRepDevlistInterface** rep_interfaces);

// Creates the OpRepDevlist message used to respond to requests to list the
// host's exported USB devices.
void CreateOpRepDevlist(const UsbDeviceDescriptor& device,
                        const UsbConfigurationDescriptor& config,
                        const std::vector<UsbInterfaceDescriptor>& interfaces,
                        OpRepDevlist* list);

// Creates the OpRepImport message used to respond to a request to attach a
// host USB device.
void CreateOpRepImport(const UsbDeviceDescriptor& device,
                       const UsbConfigurationDescriptor& config,
                       OpRepImport* rep);

// Convert the various elements of an "OpRep" message into network
// byte order and pack them into a SmartBuffer to be used for transferring along
// a socket.
SmartBuffer PackOpHeader(OpHeader header);
SmartBuffer PackOpRepDevice(OpRepDevice device);
SmartBuffer PackOpRepDevlistHeader(OpRepDevlistHeader devlist_header);
SmartBuffer PackOpRepDevlist(OpRepDevlist devlist);
SmartBuffer PackOpRepImport(OpRepImport import);

// Convert |header| into host uint8_t order.
void UnpackOpHeader(OpHeader* header);

#endif  // __OP_COMMANDS_H__
