// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escl_manager.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <crypto/random.h>

#include "xml_util.h"

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

// Generates a hyphenated UUID v4 (random).
// An example UUID looks like "0b2cdf31-edee-4246-a1ad-07bbe754856b"
std::string GenerateUUID() {
  // We only need 16 bytes of randomness, not 32, but using 32 makes the rest of
  // the code a little clearer.
  std::vector<uint8_t> bytes(32);
  crypto::RandBytes(bytes.data(), bytes.size());

  std::string uuid;
  for (int i = 0; i < 32; i++) {
    if (i == 8 || i == 12 || i == 16 || i == 20)
      uuid += "-";

    uint8_t val = bytes[i] % 16;
    if (i == 12) {
      val = 4;
    } else if (i == 16) {
      val &= ~0x4;
      val |= 0x8;
    }

    if (0 <= val && val <= 9)
      uuid += '0' + val;
    else
      uuid += 'a' + (val - 10);
  }
  return uuid;
}

std::string JobStateAsString(JobState state) {
  switch (state) {
    case kCanceled:
      return "Canceled";
    case kCompleted:
      return "Completed";
    case kPending:
      return "Pending";
  }
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

EsclManager::EsclManager(ScannerCapabilities scanner_capabilities,
                         const base::FilePath& document_path)
    : scanner_capabilities_(std::move(scanner_capabilities)),
      document_path_(document_path) {}

HttpResponse EsclManager::HandleEsclRequest(const HttpRequest& request,
                                            const SmartBuffer& request_body) {
  if (request.method == "GET" && request.uri == "/eSCL/ScannerCapabilities") {
    HttpResponse response;
    response.status = "200 OK";
    response.headers["Content-Type"] = "text/xml";
    response.body.Add(ScannerCapabilitiesAsXml(scanner_capabilities_));
    return response;
  } else if (request.method == "GET" && request.uri == "/eSCL/ScannerStatus") {
    HttpResponse response;
    response.status = "200 OK";
    response.headers["Content-Type"] = "text/xml";
    response.body.Add(ScannerStatusAsXml(status_));
    return response;
  } else if (request.method == "POST" && request.uri == "/eSCL/ScanJobs") {
    return HandleCreateScanJob(request_body);
  } else if (request.method == "GET" &&
             base::StartsWith(request.uri, "/eSCL/ScanJobs/",
                              base::CompareCase::SENSITIVE)) {
    return HandleGetNextDocument(request.uri);
  } else if (request.method == "DELETE" &&
             base::StartsWith(request.uri, "/eSCL/ScanJobs/",
                              base::CompareCase::SENSITIVE)) {
    return HandleDeleteJob(request.uri);
  } else if (request.uri == "/eSCL/ScannerCapabilities" ||
             request.uri == "/eSCL/ScannerStatus" ||
             base::StartsWith(request.uri, "/eSCL/ScanJobs",
                              base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Unexpected request method " << request.method
               << " for endpoint " << request.uri;
    HttpResponse response;
    response.status = "405 Method Not Allowed";
    return response;
  } else {
    LOG(ERROR) << "Unknown eSCL endpoint " << request.uri << " (method is "
               << request.method << ")";
    HttpResponse response;
    response.status = "404 Not Found";
    return response;
  }
}

// Generates an HTTP response for a POST request to the /eSCL/ScanJobs endpoint.
HttpResponse EsclManager::HandleCreateScanJob(const SmartBuffer& request_body) {
  HttpResponse response;

  base::Optional<ScanSettings> settings =
      ScanSettingsFromXml(request_body.contents());
  if (!settings) {
    LOG(ERROR) << "Could not parse ScanSettings from request body";
    response.status = "415 Unsupported Media Type";
    return response;
  }
  std::string uuid = GenerateUUID();
  JobInfo job;
  job.created = base::TimeTicks::Now();
  job.state = kPending;
  status_.jobs[uuid] = job;

  response.status = "201 Created";
  response.headers["Location"] = "/eSCL/ScanJobs/" + uuid;
  response.headers["Pragma"] = "no-cache";
  return response;
}

// Generates an HTTP response containing scan data for a previously created
// scan job.
// The URI should be formatted as:
//   "/eSCL/ScanJobs/0b2cdf31-edee-4246-a1ad-07bbe754856b/NextDocument"
HttpResponse EsclManager::HandleGetNextDocument(const std::string& uri) {
  HttpResponse response;
  std::vector<std::string> tokens =
      base::SplitString(uri, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 5 || tokens[4] != "NextDocument") {
    LOG(ERROR) << "Malformed GET ScanJobs request URI: " << uri;
    response.status = "405 Method Not Allowed";
    return response;
  }

  std::string uuid = tokens[3];
  if (status_.jobs.count(uuid) == 0) {
    LOG(ERROR) << "No job found with uuid: " << uuid;
    response.status = "404 Not Found";
    return response;
  }

  JobInfo info = status_.jobs[uuid];
  switch (info.state) {
    case kCanceled:
    case kCompleted:
      LOG(INFO) << "Not providing NextDocument for "
                << JobStateAsString(info.state) << " job.";
      response.status = "404 Not Found";
      break;
    case kPending: {
      status_.jobs[uuid].state = kCompleted;
      std::string document;
      if (!base::ReadFileToString(document_path_, &document)) {
        LOG(ERROR) << "Failed to read document at " << document_path_
                   << ", sending empty response";
      }
      response.status = "200 OK";
      response.headers["Content-Type"] = "image/jpeg";
      response.body.Add(document);
      break;
    }
  }

  return response;
}

// Generates an HTTP response to a request to delete the ScanJob at |uri|.
HttpResponse EsclManager::HandleDeleteJob(const std::string& uri) {
  HttpResponse response;
  std::vector<std::string> tokens =
      base::SplitString(uri, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 4) {
    LOG(ERROR) << "Malformed DELETE ScanJobs request URI: " << uri;
    response.status = "405 Method Not Allowed";
    return response;
  }

  std::string uuid = tokens[3];
  size_t erased = status_.jobs.erase(uuid);
  if (erased == 1) {
    response.status = "200 OK";
  } else {
    response.status = "404 Not Found";
  }
  return response;
}
