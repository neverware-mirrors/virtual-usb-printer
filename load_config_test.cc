// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "load_config.h"

#include "device_descriptors.h"
#include "usbip_constants.h"
#include "value_util.h"

#include <vector>
#include <map>

#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(GetByteValue, ValidValue) {
  const std::string json_contents1 = R"(
    { "test_byte": 8 }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  const base::DictionaryValue* dict1 = GetDictionary(value1.get());
  uint8_t expected1 = 8;
  EXPECT_EQ(GetByteValue(dict1, "test_byte"), expected1);

  const std::string json_contents2 = R"(
    { "test_byte": 255 }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  const base::DictionaryValue* dict2 = GetDictionary(value2.get());
  uint8_t expected2 = 255;
  EXPECT_EQ(GetByteValue(dict2, "test_byte"), expected2);
}

TEST(GetByteValue, InvalidPath) {
  const std::string json_contents = R"(
    { "test_byte": 8 }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetByteValue(dict, "wrong_path"), "Failed to extract path");
}

TEST(GetByteValue, InvalidValue) {
  const std::string json_contents = R"(
    { "test_byte": 256 }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetByteValue(dict, "test_byte"),
               "Extracted value.*is too large");
}

TEST(GetWordValue, ValidValue) {
  const std::string json_contents1 = R"(
    { "test_byte": 8 }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  const base::DictionaryValue* dict1 = GetDictionary(value1.get());
  uint16_t expected1 = 8;
  EXPECT_EQ(GetWordValue(dict1, "test_byte"), expected1);

  const std::string json_contents2 = R"(
    { "test_byte": 65535 }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  const base::DictionaryValue* dict2 = GetDictionary(value2.get());
  uint16_t expected2 = 65535;
  EXPECT_EQ(GetWordValue(dict2, "test_byte"), expected2);
}

TEST(GetWordValue, InvalidPath) {
  const std::string json_contents = R"(
    { "test_byte": 8 }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetWordValue(dict, "wrong_path"), "Failed to extract path");
}

TEST(GetWordValue, InvalidValue) {
  const std::string json_contents = R"(
    { "test_byte": 65536 }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetWordValue(dict, "test_byte"),
               "Extracted value.*is too large");
}

TEST(GetDeviceDescriptor, ValidDescriptor) {
  const std::string json_contents = R"(
    {
      "device_descriptor": {
        "bLength": 18,
        "bDescriptorType": 1,
        "bcdUSB": 272,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 8,
        "idVendor": 1193,
        "idProduct": 10216,
        "bcdDevice": 0,
        "iManufacturer": 1,
        "iProduct": 2,
        "iSerialNumber": 1,
        "bNumConfigurations": 1
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const UsbDeviceDescriptor expected(18, 1, 272, 0, 0, 0, 8, 1193, 10216, 0, 1,
                                     2, 1, 1);
  EXPECT_EQ(GetDeviceDescriptor(dict), expected);
}

TEST(GetDeviceDescriptor, InvalidBLength) {
  const std::string json_contents = R"(
    {
      "device_descriptor": {
        "bLength": 17,
        "bDescriptorType": 1,
        "bcdUSB": 272,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 8,
        "idVendor": 1193,
        "idProduct": 10216,
        "bcdDevice": 0,
        "iManufacturer": 1,
        "iProduct": 2,
        "iSerialNumber": 1,
        "bNumConfigurations": 1
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetDeviceDescriptor(dict), "bLength value.*is not the same");
}

TEST(GetConfigurationDescriptor, ValidDescriptor) {
  const std::string json_contents = R"(
    {
      "configuration_descriptor": {
        "bLength": 9,
        "bDescriptorType": 2,
        "wTotalLength": 32,
        "bNumInterfaces": 1,
        "bConfigurationValue": 1,
        "iConfiguration": 0,
        "bmAttributes": 128,
        "bMaxPower": 0
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const UsbConfigurationDescriptor expected(9, 2, 32, 1, 1, 0, 128, 0);
  EXPECT_EQ(GetConfigurationDescriptor(dict), expected);
}

TEST(GetConfigurationDescriptor, InvalidBLength) {
  const std::string json_contents = R"(
    {
      "configuration_descriptor": {
        "bLength": 10,
        "bDescriptorType": 2,
        "wTotalLength": 32,
        "bNumInterfaces": 1,
        "bConfigurationValue": 1,
        "iConfiguration": 0,
        "bmAttributes": 128,
        "bMaxPower": 0
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetConfigurationDescriptor(dict),
               "bLength value.*is not the same");
}

TEST(GetDeviceQualifierDescriptor, ValidDescriptor) {
  const std::string json_contents = R"(
    {
      "device_qualifier_descriptor": {
        "bLength": 10,
        "bDescriptorType": 6,
        "bcdUSB": 512,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 8,
        "bNumConfigurations": 1,
        "bReserved": 0
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const UsbDeviceQualifierDescriptor expected(10, 6, 512, 0, 0, 0, 8, 1, 0);
  EXPECT_EQ(GetDeviceQualifierDescriptor(dict), expected);
}

TEST(GetDeviceQualifierDescriptor, InvalidBLength) {
  const std::string json_contents = R"(
    {
      "device_qualifier_descriptor": {
        "bLength": 100,
        "bDescriptorType": 6,
        "bcdUSB": 512,
        "bDeviceClass": 0,
        "bDeviceSubClass": 0,
        "bDeviceProtocol": 0,
        "bMaxPacketSize0": 8,
        "bNumConfigurations": 1,
        "bReserved": 0
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetDeviceQualifierDescriptor(dict),
               "bLength value.*is not the same");
}

TEST(GetInterfaceDescriptors, ValidDescriptors) {
  const std::string json_contents = R"(
    {
      "interface_descriptors": [{
        "bLength": 9,
        "bDescriptorType": 4,
        "bInterfaceNumber": 0,
        "bAlternateSetting": 0,
        "bNumEndpoints": 2,
        "bInterfaceClass": 7,
        "bInterfaceSubClass": 1,
        "bInterfaceProtocol": 2,
        "iInterface": 0,
        "endpoints": []
      }]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const std::vector<UsbInterfaceDescriptor> expected = {
      UsbInterfaceDescriptor(9, 4, 0, 0, 2, 7, 1, 2, 0)};
  EXPECT_EQ(GetInterfaceDescriptors(dict), expected);
}

TEST(GetInterfaceDescriptors, InvalidBLength) {
  const std::string json_contents = R"(
    {
      "interface_descriptors": [{
        "bLength": 2,
        "bDescriptorType": 4,
        "bInterfaceNumber": 0,
        "bAlternateSetting": 0,
        "bNumEndpoints": 2,
        "bInterfaceClass": 7,
        "bInterfaceSubClass": 1,
        "bInterfaceProtocol": 2,
        "iInterface": 0,
        "endpoints": []
      }]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetInterfaceDescriptors(dict), "bLength value.*is not the same");
}

TEST(GetEndpointDescriptor, ValidDescriptor) {
  const std::string json_contents = R"(
    {
      "bLength": 7,
      "bDescriptorType": 5,
      "bEndpointAddress": 129,
      "bmAttributes": 2,
      "wMaxPacketSize": 512,
      "bInterval": 0
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const UsbEndpointDescriptor expected(7, 5, 129, 2, 512, 0);
  EXPECT_EQ(GetEndpointDescriptor(dict), expected);
}

TEST(GetEndpointDescriptor, InvalidBLength) {
  const std::string json_contents = R"(
    {
      "bLength": 8,
      "bDescriptorType": 5,
      "bEndpointAddress": 129,
      "bmAttributes": 2,
      "wMaxPacketSize": 512,
      "bInterval": 0
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  EXPECT_DEATH(GetEndpointDescriptor(dict), "bLength value.*is not the same");
}

TEST(GetEndpointDescriptorMap, ValidDescriptor) {
  const std::string json_contents = R"(
    {
      "interface_descriptors": [{
        "bLength": 9,
        "bDescriptorType": 4,
        "bInterfaceNumber": 0,
        "bAlternateSetting": 0,
        "bNumEndpoints": 2,
        "bInterfaceClass": 7,
        "bInterfaceSubClass": 1,
        "bInterfaceProtocol": 2,
        "iInterface": 0,
        "endpoints": [{
          "bLength": 7,
          "bDescriptorType": 5,
          "bEndpointAddress": 1,
          "bmAttributes": 2,
          "wMaxPacketSize": 512,
          "bInterval": 0
        }, {
          "bLength": 7,
          "bDescriptorType": 5,
          "bEndpointAddress": 129,
          "bmAttributes": 2,
          "wMaxPacketSize": 512,
          "bInterval": 0
        }]
      }]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> expected = {
      {0,
       {UsbEndpointDescriptor(7, 5, 1, 2, 512, 0),
        UsbEndpointDescriptor(7, 5, 129, 2, 512, 0)}}};
  EXPECT_EQ(GetEndpointDescriptorMap(dict), expected);
}

TEST(ConvertStringToStringDescriptor, ValidString) {
  const std::string string_descriptor = "Virtual USB Printer";
  const char size = static_cast<char>(string_descriptor.size()) * 2 + 2;
  const std::vector<char> expected = {
    size, USB_DESCRIPTOR_STRING,
    'V', 0x00, 'i', 0x00, 'r', 0x00, 't', 0x00, 'u', 0x00, 'a', 0x00, 'l', 0x00,
    ' ', 0x00, 'U', 0x00, 'S', 0x00, 'B', 0x00, ' ', 0x00, 'P', 0x00, 'r', 0x00,
    'i', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00};
  EXPECT_EQ(ConvertStringToStringDescriptor(string_descriptor), expected);
}

TEST(ConvertStringToStringDescriptor, MaxLengthString) {
  const std::string string_descriptor(126, 's');
  const char size = static_cast<char>(string_descriptor.size()) * 2 + 2;
  const std::vector<char> result =
      ConvertStringToStringDescriptor(string_descriptor);

  EXPECT_EQ(result[0], size);
  EXPECT_EQ(result[1], USB_DESCRIPTOR_STRING);
  for (int i = 2; i < size; i += 2) {
    EXPECT_EQ(result[i], 's');
    EXPECT_EQ(result[i+1], 0x00);
  }
}

// Death test for when the provided string is too large.
TEST(ConvertStringToStringDescriptor, InvalidString) {
  // A string descriptor uses 2 bytes per character, and also requires an
  // additional 2 bytes to store the length and language code. For this reason
  // the maximum number of actual characters in a string descriptor is 126.
  //
  // Examples:
  //   126 * 2 + 2 = 254  // OK!
  //   127 * 2 + 2 = 256  // TOO LARGE!
  const std::string string_descriptor(127, 's');
  EXPECT_DEATH(ConvertStringToStringDescriptor(string_descriptor),
               ".* is too large to fit into a string descriptor");
}

TEST(GetStringDescriptors, ValidDescriptors) {
  const std::string json_contents = R"(
    {
      "language_descriptor": {
        "bLength": 4,
        "bDescriptorType": 3,
        "langID1": 9,
        "langID2": 4
      },
      "string_descriptors": [
        "DavieV",
        "Virtual USB Printer"
      ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());

  const std::vector<char> expected_language_descriptor = {0x04, 0x03, 0x09,
                                                          0x04};
  const std::vector<char> expected_string_descriptor1 = {
      14,  USB_DESCRIPTOR_STRING,
      'D', 0x00, 'a', 0x00, 'v', 0x00, 'i', 0x00, 'e', 0x00, 'V', 0x00};
  const std::vector<char> expected_string_descriptor2 = {
      40, USB_DESCRIPTOR_STRING,
      'V',  0x00, 'i',  0x00, 'r',  0x00, 't',  0x00, 'u',  0x00, 'a',  0x00,
      'l',  0x00, ' ',  0x00, 'U',  0x00, 'S',  0x00, 'B',  0x00, ' ',  0x00,
      'P',  0x00, 'r',  0x00, 'i',  0x00, 'n',  0x00, 't',  0x00, 'e',  0x00,
      'r',  0x00};
  std::vector<std::vector<char>> expected = {expected_language_descriptor,
                                             expected_string_descriptor1,
                                             expected_string_descriptor2};
  EXPECT_EQ(GetStringDescriptors(dict), expected);
}

TEST(GetIEEEDeviceId, ValidDeviceId) {
  const std::string json_contents = R"(
    {
      "ieee_device_id": {
        "bLength1": 0,
        "bLength2": 26,
        "message": "MFG:DV3;CMD:PDF;MDL:VTL;"
      }
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  const base::DictionaryValue* dict = GetDictionary(value.get());
  const std::vector<char> expected = {
      0,   26,  'M', 'F', 'G', ':', 'D', 'V', '3', ';', 'C', 'M', 'D',
      ':', 'P', 'D', 'F', ';', 'M', 'D', 'L', ':', 'V', 'T', 'L', ';'};
  EXPECT_EQ(GetIEEEDeviceId(dict), expected);
}

}  // namespace
