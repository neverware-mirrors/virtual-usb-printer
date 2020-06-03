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
    "    [--scanner_capabilities_path=<path>]";

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

// Attempts to initialize an EsclManager.
// Parses |capabilities_path| into a JSON object in order to do so, returning
// nullopt if that parsing fails.
base::Optional<EsclManager> InitializeEsclManager(
    const std::string& capabilities_path) {
  if (capabilities_path.empty()) {
    return EsclManager();
  }

  std::string capabilities_string;
  if (!base::ReadFileToString(base::FilePath(capabilities_path),
                              &capabilities_string)) {
    LOG(ERROR) << "Failed to read " << capabilities_path;
    return base::nullopt;
  }

  // TODO(crbug/1054279): Use new Read API after libchrome uprev.
  std::unique_ptr<base::Value> capabilities_json =
      base::JSONReader::ReadDeprecated(capabilities_string);
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

  return EsclManager(std::move(capabilities.value()));
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(descriptors_path, "", "Path to descriptors JSON file");
  DEFINE_string(record_doc_path, "", "Path to file to record document to");
  DEFINE_string(attributes_path, "", "Path to IPP attributes JSON file");
  DEFINE_string(scanner_capabilities_path, "",
                "Path to eSCL ScannerCapabilities JSON file");

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
  // TODO(crbug/1054279): migrate to base::JSONReader::Read after uprev.
  std::unique_ptr<base::Value> descriptors =
      json_reader.ReadDeprecated(*descriptors_contents);
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

  base::FilePath document_output_path(FLAGS_record_doc_path);

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
    // TODO(crbug/1054279): migrate to base::JSONReader::Read after uprev.
    attributes = json_reader.ReadDeprecated(*attributes_contents);
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

    ipp_manager =
        IppManager(operation_attributes, printer_attributes, job_attributes,
                   unsupported_attributes, document_output_path);
  }

  base::Optional<EsclManager> escl_manager =
      InitializeEsclManager(FLAGS_scanner_capabilities_path);
  if (!escl_manager.has_value())
    return 1;

  UsbPrinter printer(usb_descriptors, document_output_path,
                     std::move(ipp_manager), std::move(escl_manager.value()));

  Server server(std::move(printer));
  server.Run();
}
