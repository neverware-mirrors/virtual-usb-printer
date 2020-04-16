// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __IPP_MANAGER_H__
#define __IPP_MANAGER_H__

#include <vector>

#include <base/files/file_path.h>

#include "ipp_util.h"
#include "smart_buffer.h"

// This class is responsible for generating responses to IPP requests sent over
// USB.
class IppManager {
 public:
  IppManager() = default;
  IppManager(std::vector<IppAttribute> operation_attributes,
             std::vector<IppAttribute> printer_attributes,
             std::vector<IppAttribute> job_attributes,
             std::vector<IppAttribute> unsupported_attributes,
             const base::FilePath& document_output_path);

  // Returns a standard response based on the operation specified in
  // |ipp_header|.
  SmartBuffer HandleIppRequest(const IppHeader& ipp_header,
                               const SmartBuffer& body) const;

  // Result returned in the |operation_id| field of an IppHeader when the
  // operation was successful.
  static const uint16_t kSuccessStatus;

 private:
  SmartBuffer HandleValidateJob(const IppHeader& request_header) const;
  SmartBuffer HandleCreateJob(const IppHeader& request_header) const;
  SmartBuffer HandleSendDocument(const IppHeader& request_header,
                                 const SmartBuffer& body) const;
  SmartBuffer HandleGetJobAttributes(const IppHeader& request_header) const;
  SmartBuffer HandleGetPrinterAttributes(const IppHeader& ipp_header) const;

  // Constant attributes that will be returned in response to requests from the
  // client.
  std::vector<IppAttribute> operation_attributes_;
  std::vector<IppAttribute> printer_attributes_;
  std::vector<IppAttribute> job_attributes_;

  // TODO(valleau): Look into making these attributes dynamic as we should only
  // report unsupported attributes if they were requested by the client.
  std::vector<IppAttribute> unsupported_attributes_;

  base::FilePath document_output_path_;
};

#endif  // __IPP_MANAGER_H__
