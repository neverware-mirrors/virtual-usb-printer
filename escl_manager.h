// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ESCL_MANAGER_H__
#define ESCL_MANAGER_H__

#include <string>
#include <vector>

#include <base/containers/flat_map.h>
#include <base/files/file_path.h>
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

  // The scan properties supported by the input sources for this scanner.
  // We require that a virtual scanner supports the Platen (flatbed) as an input
  // source, but supporting the Automatic Document Feeder (ADF) is optional.
  SourceCapabilities platen_capabilities;
  base::Optional<SourceCapabilities> adf_capabilities;
};

base::Optional<ScannerCapabilities> CreateScannerCapabilitiesFromConfig(
    const base::Value& config);

// A particular region of a document to return in a scan.
struct ScanRegion {
  // The units that the other values in the struct are measured in. Some
  // permitted values: "Micrometers", "Pixels", "TenThousandthsOfInches",
  // "Percent", "Other", "Unknown", and anything matching the regex
  // "\w+:[\w_\=\.]+".
  // See also:
  // http://www.pwg.org/mfd/navigate/PwgSmRev1-185_ContentRegionUnitsType.html
  std::string units;
  int height;
  int width;
  int x_offset;
  int y_offset;
};

// What color setting to use for an incoming scan.
enum ColorMode {
  // 1 bit black and white.
  kBlackAndWhite,

  // 8 bit grayscale.
  kGrayscale,

  // 24 bit color.
  kRGB,
};

// The information contained in a request to create a new scan job.
// This is parsed from incoming requests to the ScanJobs endpoint.
struct ScanSettings {
  // The format to return the scan data in, for example 'image/jpeg',
  // 'image/tiff, etc.
  // See also
  // http://www.pwg.org/mfd/navigatePJT/PwgPrintJobTicket_v1.0_DocumentFormatType.html
  std::string document_format;

  ColorMode color_mode;

  // The source from which to read the scan data, for example 'Platen', 'ADF',
  // 'FilmReader'.
  // See also
  // http://www.pwg.org/mfd/navigatePJT/PwgPrintJobTicket_v1.0_InputSourceType.html
  std::string input_source;

  // Frequently the X and Y resolution are the same.
  int x_resolution;
  int y_resolution;

  // All of the regions of the document that we want to scan. Most often,
  // there is only one of these.
  std::vector<ScanRegion> regions;
};

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
// The |document_path| parameter specifies the path to the scan data that
// should be reported to clients within HandleGetNextDocument.
class EsclManager {
 public:
  EsclManager() = default;
  EsclManager(ScannerCapabilities scanner_capabilities,
              const base::FilePath& document_path);

  // Generates an HTTP response for the given HttpRequest and request body.
  // If |request| is not a valid eSCL request (for example, invalid endpoint
  // or request method), the response will be an error response.
  HttpResponse HandleEsclRequest(const HttpRequest& request,
                                 const SmartBuffer& request_body);

 private:
  HttpResponse HandleCreateScanJob(const SmartBuffer& request_body);
  HttpResponse HandleGetNextDocument(const std::string& uri);
  HttpResponse HandleDeleteJob(const std::string& uri);

  ScannerCapabilities scanner_capabilities_;
  ScannerStatus status_;
  base::FilePath document_path_;
};

#endif  // ESCL_MANAGER_H__
