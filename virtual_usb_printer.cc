// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

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
#include "ipp_manager.h"
#include "load_config.h"
#include "server.h"
#include "usb_printer.h"
#include "usbip.h"
#include "usbip_constants.h"
#include "value_util.h"

namespace {

constexpr char kUsage[] =
    "virtual_usb_printer --descriptors_path=<path> [--record_doc_path=<path>] "
    "[--attributes_path=<path>]";

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
  DEFINE_string(attributes_path, "", "Path to IPP attributes JSON file");

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

  IppManager ipp_manager;
  // Load ipp attributes if |FLAGS_attributes_path| is set.
  std::unique_ptr<base::Value> attributes;
  if (!FLAGS_attributes_path.empty()) {
    base::Optional<std::string> attributes_contents =
        GetJSONContents(FLAGS_attributes_path);
    if (!attributes_contents.has_value()) {
      LOG(ERROR) << "Failed to load file contents for "
                 << FLAGS_attributes_path;
      return 1;
    }
    attributes = json_reader.Read(*attributes_contents);
    if (attributes == nullptr) {
      LOG(ERROR) << "Failed to parse " << FLAGS_attributes_path;
      return 1;
    }

    std::vector<IppAttribute> operation_attributes =
        GetAttributes(attributes.get(), kOperationAttributes);
    std::vector<IppAttribute> printer_attributes =
        GetAttributes(attributes.get(), kPrinterAttributes);
    std::vector<IppAttribute> job_attributes =
        GetAttributes(attributes.get(), kJobAttributes);
    std::vector<IppAttribute> unsupported_attributes =
        GetAttributes(attributes.get(), kUnsupportedAttributes);

    ipp_manager = IppManager(operation_attributes, printer_attributes,
                             job_attributes, unsupported_attributes);
  }

  UsbPrinter printer(usb_descriptors, std::move(ipp_manager));
  if (!FLAGS_record_doc_path.empty() &&
      !printer.SetupRecordDocument(FLAGS_record_doc_path)) {
    LOG(ERROR) << "Failed to open file: " << FLAGS_record_doc_path;
    LOG(ERROR) << "Error code: " << printer.FileError();
    return 1;
  }

  Server server(std::move(printer));
  server.Run();
}
