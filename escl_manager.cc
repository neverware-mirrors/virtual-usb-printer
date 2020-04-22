// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escl_manager.h"

#include <utility>

#include <base/logging.h>

namespace {

base::Optional<std::vector<std::string>> ExtractStringList(
    const base::Value& root, const std::string& config_name) {
  const base::Value* value =
      root.FindKeyOfType(config_name, base::Value::Type::LIST);
  if (!value) {
    LOG(ERROR) << "Config is missing " << config_name << " settings";
    return base::nullopt;
  }
  std::vector<std::string> config_list;
  for (const base::Value& v : value->GetList()) {
    if (!v.is_string()) {
      LOG(ERROR) << config_name << " value expected string, not " << v.type();
      return base::nullopt;
    }
    config_list.push_back(v.GetString());
  }
  return config_list;
}

base::Optional<std::vector<int>> ExtractIntList(
    const base::Value& root, const std::string& config_name) {
  const base::Value* value =
      root.FindKeyOfType(config_name, base::Value::Type::LIST);
  if (!value) {
    LOG(ERROR) << "Config is missing " << config_name << " settings";
    return base::nullopt;
  }
  std::vector<int> config_list;
  for (const base::Value& v : value->GetList()) {
    if (!v.is_int()) {
      LOG(ERROR) << config_name << " value expected int, not " << v.type();
      return base::nullopt;
    }
    config_list.push_back(v.GetInt());
  }
  return config_list;
}

base::Optional<SourceCapabilities> CreateSourceCapabilitiesFromConfig(
    const base::Value& config) {
  if (!config.is_dict()) {
    LOG(ERROR) << "Cannot initialize SourceCapabilities from non-dict value";
    return base::nullopt;
  }

  SourceCapabilities result;
  base::Optional<std::vector<std::string>> color_modes =
      ExtractStringList(config, "ColorModes");
  if (!color_modes) {
    LOG(ERROR) << "Could not find valid ColorModes config";
    return base::nullopt;
  }
  result.color_modes = color_modes.value();

  base::Optional<std::vector<std::string>> formats =
      ExtractStringList(config, "DocumentFormats");
  if (!formats) {
    LOG(ERROR) << "Could not find valid DocumentFormats config";
    return base::nullopt;
  }
  result.formats = formats.value();

  base::Optional<std::vector<int>> resolutions =
      ExtractIntList(config, "Resolutions");
  if (!resolutions) {
    LOG(ERROR) << "Could not find valid Resolutions config";
    return base::nullopt;
  }
  result.resolutions = resolutions.value();

  return result;
}

}  // namespace

base::Optional<ScannerCapabilities> CreateScannerCapabilitiesFromConfig(
    const base::Value& config) {
  if (!config.is_dict()) {
    LOG(ERROR) << "Cannot initialize ScannerCapabilities from non-dict value";
    return base::nullopt;
  }

  ScannerCapabilities result;
  const base::Value* value = nullptr;
  value = config.FindKeyOfType("MakeAndModel", base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << "Config is missing MakeAndModel setting";
    return base::nullopt;
  }
  result.make_and_model = value->GetString();

  value = config.FindKeyOfType("SerialNumber", base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << "Config is missing SerialNumber setting";
    return base::nullopt;
  }
  result.serial_number = value->GetString();

  value = config.FindKeyOfType("Platen", base::Value::Type::DICTIONARY);
  if (!value) {
    LOG(ERROR) << "Config is missing Platen source capabilities";
    return base::nullopt;
  }

  base::Optional<SourceCapabilities> platen_capabilities =
      CreateSourceCapabilitiesFromConfig(*value);
  if (!platen_capabilities) {
    LOG(ERROR) << "Parsing Platen capabilities failed";
    return base::nullopt;
  }
  result.platen_capabilities = platen_capabilities.value();

  return result;
}

EsclManager::EsclManager(ScannerCapabilities scanner_capabilities)
    : scanner_capabilities_(std::move(scanner_capabilities)) {}
