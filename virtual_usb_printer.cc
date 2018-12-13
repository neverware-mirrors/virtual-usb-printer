// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include <cstdio>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/optional.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "device_descriptors.h"
#include "load_config.h"
#include "server.h"
#include "usbip.h"
#include "usbip_constants.h"
#include "usb_printer.h"
#include "value_util.h"

namespace {

constexpr char kUsage[] =
    "virtual_usb_printer --descriptors_path=<path> [--record_doc_path=<path>]";

// Create a new UsbDescriptors object using the USB descriptors defined in
// |printer_config|.
UsbDescriptors CreateUsbDescriptors(
    const base::DictionaryValue* printer_config) {
  UsbDeviceDescriptor device = GetDeviceDescriptor(printer_config);
  UsbConfigurationDescriptor configuration =
      GetConfigurationDescriptor(printer_config);
  UsbDeviceQualifierDescriptor qualifier =
      GetDeviceQualifierDescriptor(printer_config);
  std::vector<UsbInterfaceDescriptor> interfaces =
      GetInterfaceDescriptors(printer_config);
  std::map<uint8_t, std::vector<UsbEndpointDescriptor>> endpoint_map =
      GetEndpointDescriptorMap(printer_config);
  std::vector<std::vector<char>> strings = GetStringDescriptors(printer_config);
  std::vector<char> ieee_device_id = GetIEEEDeviceId(printer_config);

  return UsbDescriptors(device, configuration, qualifier, strings,
                        ieee_device_id, interfaces, endpoint_map);
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");

  brillo::FlagHelper::Init(argc, argv, "Virtual USB Printer");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_descriptors_path.empty()) {
    LOG(ERROR) << kUsage;
    return 1;
  }

  base::Optional<std::string> descriptors_contents =
      GetJSONContents(FLAGS_descriptors_path);
  if (!descriptors_contents.has_value()) {
    LOG(ERROR) << "Failed to load file contents for " << FLAGS_descriptors_path;
    return 1;
  }

  base::JSONReader json_reader;
  std::unique_ptr<base::Value> descriptors =
      json_reader.Read(*descriptors_contents);
  if (descriptors == nullptr) {
    LOG(ERROR) << "Failed to parse " << FLAGS_descriptors_path;
    return 1;
  }

  base::DictionaryValue* descriptors_dictionary;
  if (!descriptors->GetAsDictionary(&descriptors_dictionary)) {
    LOG(ERROR) << "Failed to extract printer configuration as dictionary";
    return 1;
  }

  UsbDescriptors usb_descriptors = CreateUsbDescriptors(descriptors_dictionary);
  UsbPrinter printer(usb_descriptors);
  if (!FLAGS_record_doc_path.empty() &&
      !printer.SetupRecordDocument(FLAGS_record_doc_path)) {
    LOG(ERROR) << "Failed to open file: " << FLAGS_record_doc_path;
    LOG(ERROR) << "Error code: " << printer.FileError();
    return 1;
  }
  RunServer(printer);
}
