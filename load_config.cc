// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "load_config.h"

#include "device_descriptors.h"
#include "usbip_constants.h"

#include <climits>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>
#include <brillo/syslog_logging.h>

namespace {

// Represents the maximum number of characters in a USB string descriptor.
const size_t kMaxStringDescriptorSize = 126;

}  // namespace

uint8_t GetByteValue(const base::DictionaryValue* dict,
                     const std::string& path) {
  int val;
  CHECK(dict->GetInteger(path, &val))
      << "Failed to extract path" << path << " from printer config";
  CHECK_LE(val, UCHAR_MAX) << "Extracted value " << val
                           << " is too large to fit into a byte";
  return static_cast<uint8_t>(val);
}

uint16_t GetWordValue(const base::DictionaryValue* dict,
                      const std::string& path) {
  int val;
  CHECK(dict->GetInteger(path, &val))
      << "Failed to extract path " << path << " from printer config";
  CHECK_LE(val, USHRT_MAX) << "Extracted value " << val
                           << " is too large to fit into a word";
  return static_cast<uint16_t>(val);
}

const base::DictionaryValue* GetDescriptorDictionary(
    const base::DictionaryValue* printer, const std::string& path) {
  const base::DictionaryValue* descriptor;
  CHECK(printer->GetDictionary(path, &descriptor))
      << "Failed to extract " << path << " object from printer config";
  return descriptor;
}

const base::ListValue* GetDescriptorList(const base::DictionaryValue* printer,
                                         const std::string& path) {
  const base::ListValue* list;
  CHECK(printer->GetList(path, &list))
      << "Failed to extract " << path << " list from printer config";
  return list;
}

UsbDeviceDescriptor GetDeviceDescriptor(
    const base::DictionaryValue* printer) {
  const base::DictionaryValue* descriptor =
      GetDescriptorDictionary(printer, "device_descriptor");
  UsbDeviceDescriptor dev_dsc;
  dev_dsc.bLength = GetByteValue(descriptor, "bLength");
  CHECK_EQ(dev_dsc.bLength, sizeof(dev_dsc))
      << "bLength value " << dev_dsc.bLength
      << " is not the same as the size of the device descriptor";
  dev_dsc.bDescriptorType = GetByteValue(descriptor, "bDescriptorType");
  dev_dsc.bcdUSB = GetWordValue(descriptor, "bcdUSB");
  dev_dsc.bDeviceClass = GetByteValue(descriptor, "bDeviceClass");
  dev_dsc.bDeviceSubClass = GetByteValue(descriptor, "bDeviceSubClass");
  dev_dsc.bDeviceProtocol = GetByteValue(descriptor, "bDeviceProtocol");
  dev_dsc.bMaxPacketSize0 = GetByteValue(descriptor, "bMaxPacketSize0");
  dev_dsc.idVendor = GetWordValue(descriptor, "idVendor");
  dev_dsc.idProduct = GetWordValue(descriptor, "idProduct");
  dev_dsc.bcdDevice = GetWordValue(descriptor, "bcdDevice");
  dev_dsc.iManufacturer = GetByteValue(descriptor, "iManufacturer");
  dev_dsc.iProduct = GetByteValue(descriptor, "iProduct");
  dev_dsc.iSerialNumber = GetByteValue(descriptor, "iSerialNumber");
  dev_dsc.bNumConfigurations = GetByteValue(descriptor, "bNumConfigurations");
  return dev_dsc;
}

UsbConfigurationDescriptor GetConfigurationDescriptor(
    const base::DictionaryValue* printer) {
  const base::DictionaryValue* descriptor =
      GetDescriptorDictionary(printer, "configuration_descriptor");
  UsbConfigurationDescriptor cfg_dsc;
  cfg_dsc.bLength = GetByteValue(descriptor, "bLength");
  CHECK_EQ(cfg_dsc.bLength, sizeof(cfg_dsc))
      << "bLength value " << cfg_dsc.bLength
      << " is not the same as the size of the configuration descriptor";
  cfg_dsc.bDescriptorType = GetByteValue(descriptor, "bDescriptorType");
  cfg_dsc.wTotalLength = GetWordValue(descriptor, "wTotalLength");
  cfg_dsc.bNumInterfaces = GetByteValue(descriptor, "bNumInterfaces");
  cfg_dsc.bConfigurationValue = GetByteValue(descriptor, "bConfigurationValue");
  cfg_dsc.iConfiguration = GetByteValue(descriptor, "iConfiguration");
  cfg_dsc.bmAttributes = GetByteValue(descriptor, "bmAttributes");
  cfg_dsc.bMaxPower = GetByteValue(descriptor, "bMaxPower");
  return cfg_dsc;
}

UsbDeviceQualifierDescriptor GetDeviceQualifierDescriptor(
    const base::DictionaryValue* printer) {
  const base::DictionaryValue* descriptor =
      GetDescriptorDictionary(printer, "device_qualifier_descriptor");
  UsbDeviceQualifierDescriptor devq_dsc;
  devq_dsc.bLength = GetByteValue(descriptor, "bLength");
  CHECK_EQ(devq_dsc.bLength, sizeof(devq_dsc))
      << "bLength value " << devq_dsc.bLength
      << " is not the same as the size of the device qualifier descriptor";
  devq_dsc.bDescriptorType = GetByteValue(descriptor, "bDescriptorType");
  devq_dsc.bcdUSB = GetWordValue(descriptor, "bcdUSB");
  devq_dsc.bDeviceClass = GetByteValue(descriptor, "bDeviceClass");
  devq_dsc.bDeviceSubClass = GetByteValue(descriptor, "bDeviceSubClass");
  devq_dsc.bDeviceProtocol = GetByteValue(descriptor, "bDeviceProtocol");
  devq_dsc.bMaxPacketSize0 = GetByteValue(descriptor, "bMaxPacketSize0");
  devq_dsc.bNumConfigurations = GetByteValue(descriptor, "bNumConfigurations");
  devq_dsc.bReserved = GetByteValue(descriptor, "bReserved");
  return devq_dsc;
}

std::vector<UsbInterfaceDescriptor> GetInterfaceDescriptors(
    const base::DictionaryValue* printer) {
  const base::ListValue* list =
      GetDescriptorList(printer, "interface_descriptors");
  std::size_t size = list->GetSize();
  std::vector<UsbInterfaceDescriptor> interfaces(size);
  for (std::size_t i = 0; i < size; ++i) {
    const base::DictionaryValue* descriptor;
    CHECK(list->GetDictionary(i, &descriptor))
        << "Failed to extract object from list index " << i;
    interfaces[i] = GetInterfaceDescriptor(descriptor);
  }
  return interfaces;
}

UsbInterfaceDescriptor GetInterfaceDescriptor(
    const base::DictionaryValue* descriptor) {
  UsbInterfaceDescriptor intf_dsc;
  intf_dsc.bLength = GetByteValue(descriptor, "bLength");
  CHECK_EQ(intf_dsc.bLength, sizeof(intf_dsc))
      << "bLength value " << intf_dsc.bLength
      << " is not the same as the size of the interface descriptor";
  intf_dsc.bDescriptorType = GetByteValue(descriptor, "bDescriptorType");
  intf_dsc.bInterfaceNumber = GetByteValue(descriptor, "bInterfaceNumber");
  intf_dsc.bAlternateSetting = GetByteValue(descriptor, "bAlternateSetting");
  intf_dsc.bNumEndpoints = GetByteValue(descriptor, "bNumEndpoints");
  intf_dsc.bInterfaceClass = GetByteValue(descriptor, "bInterfaceClass");
  intf_dsc.bInterfaceSubClass = GetByteValue(descriptor, "bInterfaceSubClass");
  intf_dsc.bInterfaceProtocol = GetByteValue(descriptor, "bInterfaceProtocol");
  intf_dsc.iInterface = GetByteValue(descriptor, "iInterface");
  return intf_dsc;
}

std::map<uint8_t, std::vector<UsbEndpointDescriptor>> GetEndpointDescriptorMap(
    const base::DictionaryValue* printer) {
  const base::ListValue* interfaces_list =
      GetDescriptorList(printer, "interface_descriptors");
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoints_map;
  for (size_t i = 0; i < interfaces_list->GetSize(); ++i) {
    const base::DictionaryValue* interface_dictionary;
    CHECK(interfaces_list->GetDictionary(i, &interface_dictionary))
        << "Failed to extract object from list index " << i;
    UsbInterfaceDescriptor interface =
        GetInterfaceDescriptor(interface_dictionary);

    const base::ListValue* endpoints_list =
        GetDescriptorList(interface_dictionary, "endpoints");
    for (size_t j = 0; j < endpoints_list->GetSize(); ++j) {
      const base::DictionaryValue* endpoint_dictionary;
      CHECK(endpoints_list->GetDictionary(j, &endpoint_dictionary))
          << "Failed to extract the object from list index " << j;
      UsbEndpointDescriptor endpoint =
          GetEndpointDescriptor(endpoint_dictionary);

      endpoints_map[interface.bInterfaceNumber].push_back(endpoint);
    }
  }

  return endpoints_map;
}

UsbEndpointDescriptor GetEndpointDescriptor(
    const base::DictionaryValue* descriptor) {
  UsbEndpointDescriptor endp_dsc;
  endp_dsc.bLength = GetByteValue(descriptor, "bLength");
  CHECK_EQ(endp_dsc.bLength, sizeof(endp_dsc))
      << "bLength value " << endp_dsc.bLength
      << " is not the same as the size of the endpoint descriptor";
  endp_dsc.bDescriptorType = GetByteValue(descriptor, "bDescriptorType");
  endp_dsc.bEndpointAddress = GetByteValue(descriptor, "bEndpointAddress");
  endp_dsc.bmAttributes = GetByteValue(descriptor, "bmAttributes");
  endp_dsc.wMaxPacketSize = GetWordValue(descriptor, "wMaxPacketSize");
  endp_dsc.bInterval = GetByteValue(descriptor, "bInterval");
  return endp_dsc;
}

std::vector<char> ConvertStringToStringDescriptor(const std::string& str) {
  CHECK_LE(str.size(), kMaxStringDescriptorSize)
      << str << " is too large to fit into a string descriptor";
  // A string descriptor uses 2 bytes per character, and also requires an
  // additional 2 bytes to store the length and language code.
  const uint8_t size = static_cast<char>(str.size()) * 2 + 2;
  std::vector<char> descriptor(size);
  descriptor[0] = size;
  descriptor[1] = USB_DESCRIPTOR_STRING;
  int i = 2;
  for (const char c : str) {
    descriptor[i] = c;
    descriptor[i + 1] = 0x00;
    i += 2;
  }
  return descriptor;
}

std::vector<std::vector<char>> GetStringDescriptors(
    const base::DictionaryValue* printer) {
  const base::DictionaryValue* descriptor =
      GetDescriptorDictionary(printer, "language_descriptor");
  std::vector<char> language(4);
  language[0] = GetByteValue(descriptor, "bLength");
  language[1] = GetByteValue(descriptor, "bDescriptorType");
  language[2] = GetByteValue(descriptor, "langID1");
  language[3] = GetByteValue(descriptor, "langID2");

  const base::ListValue* list =
      GetDescriptorList(printer, "string_descriptors");
  std::vector<std::vector<char>> strings;
  strings.push_back(language);
  for (std::size_t i = 0; i < list->GetSize(); ++i) {
    std::string str;
    CHECK(list->GetString(i, &str))
        << "Failed to extract string from list index " << i;
    strings.emplace_back(ConvertStringToStringDescriptor(str));
  }
  return strings;
}

std::vector<char> GetIEEEDeviceId(const base::DictionaryValue* printer) {
  const base::DictionaryValue* descriptor =
      GetDescriptorDictionary(printer, "ieee_device_id");
  std::vector<char> ieee_device_id;
  ieee_device_id.push_back(
      static_cast<char>(GetByteValue(descriptor, "bLength1")));
  ieee_device_id.push_back(
      static_cast<char>(GetByteValue(descriptor, "bLength2")));
  std::string message;
  CHECK(descriptor->GetString("message", &message))
      << "Failed to extract \"message\" from ieee_device_id object";
  ieee_device_id.insert(ieee_device_id.end(), message.begin(), message.end());
  return ieee_device_id;
}
