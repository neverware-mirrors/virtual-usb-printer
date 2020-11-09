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
    "virtual_usb_printer\n"
    "    --descriptors_path=<path>\n"
    "    [--record_doc_path=<path>]\n"
    "    [--attributes_path=<path>]\n"
    "    [--scanner_capabilities_path=<path>]"
    "    [--scanner_doc_path=<path>]";

// Create a new UsbDescriptors object using the USB descriptors defined in
// |printer_config|.
UsbDescriptors CreateUsbDescriptors(const base::Value& printer_config) {
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

// Attempts to initialize an EsclManager.
// Parses |capabilities_path| into a JSON object in order to do so, returning
// nullopt if that parsing fails.
base::Optional<EsclManager> InitializeEsclManager(
    const std::string& capabilities_path, const std::string& scanner_doc_path) {
  if (capabilities_path.empty()) {
    return EsclManager();
  }

  std::string capabilities_string;
  if (!base::ReadFileToString(base::FilePath(capabilities_path),
                              &capabilities_string)) {
    LOG(ERROR) << "Failed to read " << capabilities_path;
    return base::nullopt;
  }

  base::Optional<base::Value> capabilities_json =
      base::JSONReader::Read(capabilities_string);
  if (!capabilities_json) {
    LOG(ERROR) << "Failed to parse capabilities as JSON";
    return base::nullopt;
  }

  base::Optional<ScannerCapabilities> capabilities =
      CreateScannerCapabilitiesFromConfig(*capabilities_json);
  if (!capabilities) {
    LOG(ERROR) << "Failed to initialize ScannerCapabilities";
    return base::nullopt;
  }

  base::FilePath document_path(scanner_doc_path);
  return EsclManager(std::move(capabilities.value()), document_path);
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_string(attributes_path, "", "Path to IPP attributes JSON file");
  DEFINE_string(scanner_capabilities_path, "",
                "Path to eSCL ScannerCapabilities JSON file");
  DEFINE_string(scanner_doc_path, "",
                "Path to file containing data to return from scan jobs");

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

  base::Optional<base::Value> descriptors =
      base::JSONReader::Read(*descriptors_contents);
  if (!descriptors) {
    LOG(ERROR) << "Failed to parse " << FLAGS_descriptors_path;
    return 1;
  }

  if (!descriptors->is_dict()) {
    LOG(ERROR) << "Failed to extract printer configuration as dictionary";
    return 1;
  }

  UsbDescriptors usb_descriptors = CreateUsbDescriptors(*descriptors);

  base::FilePath document_output_path(FLAGS_record_doc_path);

  IppManager ipp_manager;
  // Load ipp attributes if |FLAGS_attributes_path| is set.
  base::Optional<base::Value> result;
  if (!FLAGS_attributes_path.empty()) {
    base::Optional<std::string> attributes_contents =
        GetJSONContents(FLAGS_attributes_path);
    if (!attributes_contents.has_value()) {
      LOG(ERROR) << "Failed to load file contents for "
                 << FLAGS_attributes_path;
      return 1;
    }
    result = base::JSONReader::Read(*attributes_contents);
    if (!result) {
      LOG(ERROR) << "Failed to parse " << FLAGS_attributes_path;
      return 1;
    }

    std::vector<IppAttribute> operation_attributes =
        GetAttributes(*result, kOperationAttributes);
    std::vector<IppAttribute> printer_attributes =
        GetAttributes(*result, kPrinterAttributes);
    std::vector<IppAttribute> job_attributes =
        GetAttributes(*result, kJobAttributes);
    std::vector<IppAttribute> unsupported_attributes =
        GetAttributes(*result, kUnsupportedAttributes);

    ipp_manager =
        IppManager(operation_attributes, printer_attributes, job_attributes,
                   unsupported_attributes, document_output_path);
  }

  base::Optional<EsclManager> escl_manager = InitializeEsclManager(
      FLAGS_scanner_capabilities_path, FLAGS_scanner_doc_path);
  if (!escl_manager.has_value())
    return 1;

  UsbPrinter printer(usb_descriptors, document_output_path,
                     std::move(ipp_manager), std::move(escl_manager.value()));

  Server server(std::move(printer));
  server.Run();
}
