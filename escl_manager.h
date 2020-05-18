// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __ESCL_MANAGER_H__
#define __ESCL_MANAGER_H__

#include <string>
#include <vector>

#include <base/containers/flat_map.h>
#include <base/optional.h>
#include <base/time/time.h>
#include <base/values.h>

#include "http_util.h"

struct SourceCapabilities {
  std::vector<std::string> color_modes;
  std::vector<std::string> formats;
  std::vector<int> resolutions;
};

struct ScannerCapabilities {
  std::string make_and_model;
  std::string serial_number;

  SourceCapabilities platen_capabilities;
};

base::Optional<ScannerCapabilities> CreateScannerCapabilitiesFromConfig(
    const base::Value& config);

// The possible states for a scan job.
enum JobState {
  // The job was cancelled before it could be completed.
  kCanceled,
  // The document has been scanned, and scan data has been sent to the client.
  kCompleted,
  // The document may be in the process of scanning. Image data (if any has
  // been scanned yet) has not been sent to the client.
  kPending,
};

// The information tracked for a particular scan job.
struct JobInfo {
  // The time when the job was created.
  base::TimeTicks created;
  // The current state of the job.
  JobState state;
};

struct ScannerStatus {
  bool idle;
  // All of the scan jobs for this scanner.
  // Keys are v4 UUIDs. Scan jobs may be in any state.
  base::flat_map<std::string, JobInfo> jobs;
};

// This class is responsible for generating responses to eSCL requests sent
// over USB.
class EsclManager {
 public:
  EsclManager() = default;
  explicit EsclManager(ScannerCapabilities scanner_capabilities);

  // Generates an HTTP response for the given HttpRequest.
  // If |request| is not a valid eSCL request (for example, invalid endpoint
  // or request method), the response will be an error response.
  HttpResponse HandleEsclRequest(const HttpRequest& request) const;

 private:
  ScannerCapabilities scanner_capabilities_;
  ScannerStatus status_;
};

#endif  // __ESCL_MANAGER_H__
