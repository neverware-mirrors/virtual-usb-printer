// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_manager.h"

#include <base/files/file_util.h>
#include <base/logging.h>

const uint16_t IppManager::kSuccessStatus = 0;

IppManager::IppManager(std::vector<IppAttribute> operation_attributes,
                       std::vector<IppAttribute> printer_attributes,
                       std::vector<IppAttribute> job_attributes,
                       std::vector<IppAttribute> unsupported_attributes,
                       const base::FilePath& document_output_path)
    : operation_attributes_(operation_attributes),
      printer_attributes_(printer_attributes),
      job_attributes_(job_attributes),
      unsupported_attributes_(unsupported_attributes),
      document_output_path_(document_output_path) {}

SmartBuffer IppManager::HandleIppRequest(const IppHeader& ipp_header,
                                         const SmartBuffer& body) const {
  switch (ipp_header.operation_id) {
    case IPP_VALIDATE_JOB:
      return HandleValidateJob(ipp_header);
    case IPP_CREATE_JOB:
      return HandleCreateJob(ipp_header);
    case IPP_SEND_DOCUMENT:
      return HandleSendDocument(ipp_header, body);
    case IPP_GET_JOB_ATTRIBUTES:
      return HandleGetJobAttributes(ipp_header);
    case IPP_GET_PRINTER_ATTRIBUTES:
      return HandleGetPrinterAttributes(ipp_header);
    default:
      LOG(ERROR) << "Unknown operation id in ipp request "
                 << ipp_header.operation_id;
      return SmartBuffer();
  }
}

SmartBuffer IppManager::HandleValidateJob(
    const IppHeader& request_header) const {
  printf("HandleValidateJob %u\n", request_header.request_id);

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size =
      sizeof(response_header) + GetAttributesSize(operation_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleCreateJob(const IppHeader& request_header) const {
  LOG(INFO) << "HandleCreateJob " << request_header.request_id;

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleSendDocument(const IppHeader& request_header,
                                           const SmartBuffer& body) const {
  LOG(INFO) << "HandleSendDocument " << request_header.request_id;

  if (!document_output_path_.empty()) {
    LOG(INFO) << "Recording document...";
    int written = base::WriteFile(document_output_path_,
                                  reinterpret_cast<const char*>(body.data()),
                                  body.size());
    if (written != body.size()) {
      PLOG(ERROR) << "Failed to write document to file: "
                  << document_output_path_;
    }
  }

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleGetJobAttributes(
    const IppHeader& request_header) const {
  LOG(INFO) << "HandleGetJobAttributes " << request_header.request_id;
  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(job_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(job_attributes_, kJobAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}

SmartBuffer IppManager::HandleGetPrinterAttributes(
    const IppHeader& request_header) const {
  LOG(INFO) << "HandleGetPrinterAttributes " << request_header.request_id;

  IppHeader response_header = request_header;
  response_header.operation_id = kSuccessStatus;
  // We add 1 to the size for the end of attributes tag.
  size_t response_size = sizeof(response_header) +
                         GetAttributesSize(operation_attributes_) +
                         GetAttributesSize(printer_attributes_) + 1;
  SmartBuffer buf(response_size);
  response_header.Serialize(&buf);
  AddPrinterAttributes(operation_attributes_, kOperationAttributes, &buf);
  AddPrinterAttributes(printer_attributes_, kPrinterAttributes, &buf);
  AddEndOfAttributes(&buf);
  return buf;
}
