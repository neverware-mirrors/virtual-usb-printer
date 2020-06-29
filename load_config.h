// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __LOAD_CONFIG_H__
#define __LOAD_CONFIG_H__

#include <map>
#include <vector>
#include <string>

#include <base/values.h>

#include "device_descriptors.h"
#include "usbip_constants.h"

// Extract the uint8_t value associated with the key |path| from |dict|.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
uint8_t GetByteValue(const base::DictionaryValue* dict,
                     const std::string& path);

// Extract the uint16_t value associated with the key |path| from |dict|.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
uint16_t GetWordValue(const base::DictionaryValue* dict,
                      const std::string& path);

// Extract the descriptor JSON object associated with the key |path| from the
// |printer| config JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
const base::DictionaryValue* GetDescriptorDictionary(
    const base::DictionaryValue* printer, const std::string& path);

// Extract the list of descriptors associated with the key |path| from the
// |printer| config JSON.
// TODO(crbug.com/1099111): change base::{List,Dictionary}Value to base::Value
const base::ListValue* GetDescriptorList(const base::DictionaryValue* printer,
                                         const std::string& path);

// Extract the USB device descriptor from the given |printer| config JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
UsbDeviceDescriptor GetDeviceDescriptor(const base::DictionaryValue* printer);

// Extract the USB configuration descriptor from the given |printer| config
// JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
UsbConfigurationDescriptor GetConfigurationDescriptor(
    const base::DictionaryValue* printer);

// Extract the USB device qualifier descriptor from the given |printer| config
// JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
UsbDeviceQualifierDescriptor GetDeviceQualifierDescriptor(
    const base::DictionaryValue* printer);

// Extract each of the USB interface descriptors from the given |printer| config
// JSON and return them in a vector.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
std::vector<UsbInterfaceDescriptor> GetInterfaceDescriptors(
    const base::DictionaryValue* printer);

// Extract the values from the given interface descriptor JSON |descriptor|.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
UsbInterfaceDescriptor GetInterfaceDescriptor(
    const base::DictionaryValue* descriptor);

// Extract the interface descriptors and their associated endpoint descriptors
// to construct a mapping from interface numbers to a collection of endpoint
// descriptors.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
std::map<uint8_t, std::vector<UsbEndpointDescriptor>> GetEndpointDescriptorMap(
    const base::DictionaryValue* printer);

// Extract the USB endpoint descriptor from the given |printer| config JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
UsbEndpointDescriptor GetEndpointDescriptor(
    const base::DictionaryValue* printer);


// Converts |string| into a USB string descriptor stored in a vector of
// characters.
std::vector<char> ConvertStringToStringDescriptor(const std::string& str);

// Extract the string descriptors from the given |printer| config JSON. The
// |printer| JSON is expected to contain the key "language_descriptor" which
// represents the special language string descriptor. The following string
// descriptors are expected to be stored in a list associated with the key
// "string_descriptors".
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
std::vector<std::vector<char>> GetStringDescriptors(
    const base::DictionaryValue* printer);

// Extracts the IEEE Device ID from the given |printer| config JSON.
// TODO(crbug.com/1099111): change base::DictionaryValue to base::Value
std::vector<char> GetIEEEDeviceId(const base::DictionaryValue* printer);

#endif  // __LOAD_CONFIG_H__
