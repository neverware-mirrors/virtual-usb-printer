// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_manager.h"

#include <string>
#include <vector>

#include <base/values.h>
#include <gtest/gtest.h>

#include "ipp_util.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

class IppManagerTest : public testing::Test {
 protected:
  IppManagerTest()
      : int_value_(17),
        bool_value_(true),
        uri_value_("http://www.example.com"),
        int_attribute_(kInteger, "int attribute", &int_value_),
        bool_attribute_(kBoolean, "bool attribute", &bool_value_),
        uri_attribute_(kUri, "uri attribute", &uri_value_),
        operation_attributes_({int_attribute_}),
        printer_attributes_({bool_attribute_}),
        job_attributes_({uri_attribute_}),
        ipp_manager_(operation_attributes_,
                     printer_attributes_,
                     job_attributes_,
                     unsupported_attributes_) {
    AddPrinterAttributes(operation_attributes_, kOperationAttributes,
                         &serialized_operation_);
    AddPrinterAttributes(printer_attributes_, kPrinterAttributes,
                         &serialized_printer_);
    AddPrinterAttributes(job_attributes_, kJobAttributes, &serialized_job_);
  }

  static IppHeader CreateTestHeader() {
    IppHeader header;
    header.major = 2;
    header.minor = 0;
    header.operation_id = IPP_VALIDATE_JOB;
    header.request_id = 14;
    return header;
  }

  base::Value int_value_;
  base::Value bool_value_;
  base::Value uri_value_;

  IppAttribute int_attribute_;
  IppAttribute bool_attribute_;
  IppAttribute uri_attribute_;

  SmartBuffer serialized_operation_;
  SmartBuffer serialized_printer_;
  SmartBuffer serialized_job_;

  std::vector<IppAttribute> operation_attributes_;
  std::vector<IppAttribute> printer_attributes_;
  std::vector<IppAttribute> job_attributes_;
  std::vector<IppAttribute> unsupported_attributes_;

  IppManager ipp_manager_;
};

TEST_F(IppManagerTest, InvalidOperation) {
  IppHeader header = CreateTestHeader();
  header.operation_id = 0xFFFF;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  EXPECT_EQ(response.size(), 0);
}

TEST_F(IppManagerTest, HandleValidateJob) {
  IppHeader header = CreateTestHeader();
  header.operation_id = IPP_VALIDATE_JOB;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  base::Optional<IppHeader> response_header = IppHeader::Deserialize(&response);
  EXPECT_TRUE(response_header);
  EXPECT_EQ(response_header.value().operation_id, IppManager::kSuccessStatus);

  SmartBuffer expected_response;
  expected_response.Add(serialized_operation_);
  AddEndOfAttributes(&expected_response);
  EXPECT_EQ(response.contents(), expected_response.contents());
}

TEST_F(IppManagerTest, HandleCreateJob) {
  IppHeader header = CreateTestHeader();
  header.operation_id = IPP_CREATE_JOB;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  base::Optional<IppHeader> response_header = IppHeader::Deserialize(&response);
  EXPECT_TRUE(response_header);
  EXPECT_EQ(response_header.value().operation_id, IppManager::kSuccessStatus);

  SmartBuffer expected_response;
  expected_response.Add(serialized_operation_);
  expected_response.Add(serialized_job_);
  AddEndOfAttributes(&expected_response);
  EXPECT_EQ(response.contents(), expected_response.contents());
}

TEST_F(IppManagerTest, HandleSendDocument) {
  IppHeader header = CreateTestHeader();
  header.operation_id = IPP_SEND_DOCUMENT;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  base::Optional<IppHeader> response_header = IppHeader::Deserialize(&response);
  EXPECT_TRUE(response_header);
  EXPECT_EQ(response_header.value().operation_id, IppManager::kSuccessStatus);

  SmartBuffer expected_response;
  expected_response.Add(serialized_operation_);
  expected_response.Add(serialized_job_);
  AddEndOfAttributes(&expected_response);
  EXPECT_EQ(response.contents(), expected_response.contents());
}

TEST_F(IppManagerTest, HandleGetJobAttributes) {
  IppHeader header = CreateTestHeader();
  header.operation_id = IPP_GET_JOB_ATTRIBUTES;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  base::Optional<IppHeader> response_header = IppHeader::Deserialize(&response);
  EXPECT_TRUE(response_header);
  EXPECT_EQ(response_header.value().operation_id, IppManager::kSuccessStatus);

  SmartBuffer expected_response;
  expected_response.Add(serialized_operation_);
  expected_response.Add(serialized_job_);
  AddEndOfAttributes(&expected_response);
  EXPECT_EQ(response.contents(), expected_response.contents());
}

TEST_F(IppManagerTest, HandleGetPrinterAttributes) {
  IppHeader header = CreateTestHeader();
  header.operation_id = IPP_GET_PRINTER_ATTRIBUTES;

  SmartBuffer response = ipp_manager_.HandleIppRequest(header);
  base::Optional<IppHeader> response_header = IppHeader::Deserialize(&response);
  EXPECT_TRUE(response_header);
  EXPECT_EQ(response_header.value().operation_id, IppManager::kSuccessStatus);

  SmartBuffer expected_response;
  expected_response.Add(serialized_operation_);
  expected_response.Add(serialized_printer_);
  AddEndOfAttributes(&expected_response);
  EXPECT_EQ(response.contents(), expected_response.contents());
}
