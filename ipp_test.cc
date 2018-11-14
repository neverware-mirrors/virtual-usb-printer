// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_util.h"

#include <cups/cups.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <base/json/json_reader.h>
#include <base/values.h>

#include "smart_buffer.h"
#include "value_util.h"

namespace {

using namespace std::string_literals;

const std::vector<uint8_t> CreateByteVector(const std::string& str) {
  std::vector<uint8_t> v(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    v[i] = static_cast<uint8_t>(str[i]);
  }
  return v;
}

TEST(IppAttributeEquality, SameAttributes) {
  std::unique_ptr<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", val.get());
  IppAttribute value2("integer", "test-attribute", val.get());
  EXPECT_EQ(value1, value2);
}

TEST(IppAttributeEquality, DifferentTypes) {
  std::unique_ptr<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", val.get());
  IppAttribute value2("enum", "test-attribute", val.get());
  EXPECT_NE(value1, value2);
}

TEST(IppAttributeEquality, DifferentNames) {
  std::unique_ptr<base::Value> val = GetJSONValue("123");
  IppAttribute value1("integer", "test-attribute", val.get());
  IppAttribute value2("integer", "fake-attribute", val.get());
  EXPECT_NE(value1, value2);
}

// TODO(valleau): Add a test case for inequality when the actual value members
// differ once chromelib is updated and the equality operator for base::Value is
// defined.

TEST(GetIppHeader, VerifyHeaderExtraction) {
  const std::string http_header =
      // HTTP header.
      "POST /ipp/print HTTP/1.1\x0d\x0a"
      "Content-Length: 116\x0d\x0a"
      "Content-Type: application/ipp\x0d\x0a"
      "Host: localhost:0\x0d\x0a"
      "User-Agent: CUPS/2.1.4 (Linux 4.14.62; x86_64) IPP/2.0\x0d\x0a"
      "Expect: 100-continue\x0d\x0a\x0d\x0a"
      // IPP header.
      "\x02\x00\x00\x0b\x00\x00\x00\x03"
      // IPP attributes.
      "\x01\x47\x00\x12"
      "attributes-charset"
      "\x00\x05"
      "utf-8"
      "\x48\x00\x1b"
      "attributes-natural-language"
      "\x00\x05"
      "en-us"
      "\x45\x00\x0b"
      "printer-uri"
      "\x00\x19"
      "ipp://04a9_27e8/ipp/print\x03"s;

  std::vector<uint8_t> bytes = CreateByteVector(http_header);
  SmartBuffer buffer(bytes);

  IppHeader ipp_header = GetIppHeader(buffer);

  EXPECT_EQ(ipp_header.major, 2);
  EXPECT_EQ(ipp_header.minor, 0);
  EXPECT_EQ(ipp_header.operation_id, 0x000b);
  EXPECT_EQ(ipp_header.request_id, 3);
}

TEST(IsHttpChunkedHeader, ContainsChunkedEncoding) {
  const std::string http_header =
      "POST /ipp/print HTTP/1.1\x0d\x0a"
      "Content-Type: application/ipp\x0d\x0a"
      "Date: Mon, 12 Nov 2018 19:17:31 GMT\x0d\x0a"
      "Host: localhost:0\x0d\x0a"
      "Transfer-Encoding: chunked\x0d\x0a"
      "User-Agent: CUPS/2.2.8 (Linux 4.14.82; x86_64) IPP/2.0\x0d\x0a"
      "Expect: 100-continue\x0d\x0a\x0d\x0a"s;

  std::vector<uint8_t> bytes = CreateByteVector(http_header);
  SmartBuffer buf(bytes);
  EXPECT_TRUE(IsHttpChunkedMessage(buf));
}

TEST(ContainsHttpBody, ContainsHeader) {
  const std::string message =
      "POST /ipp/print HTTP/1.1\x0d\x0a\x0d\x0a"
      // message body.
      "\x02\x00\x00\x0b\x00\x00\x00\x01"s;

  // We expect ContainsHttpBody to return true since |message| contains an HTTP
  // header with contents immediately following the header.
  std::vector<uint8_t> bytes = CreateByteVector(message);
  SmartBuffer buf(bytes);
  EXPECT_TRUE(ContainsHttpBody(buf));
}

TEST(ContainsHttpBody, NoBody) {
  const std::string message = "POST /ipp/print HTTP/1.1\x0d\x0a\x0d\x0a";

  // We expect ContainsHttpBody to return false since |message| contains an HTTP
  // header with nothing immediately following it.
  std::vector<uint8_t> bytes = CreateByteVector(message);
  SmartBuffer buf(bytes);
  EXPECT_FALSE(ContainsHttpBody(buf));
}

TEST(ContainsHttpBody, NoHttpHeader) {
  const std::string message = "\x02\x00\x00\x0b\x00\x00\x00\x01"s;

  // Since |message| does not contain an HTTP header, we assume that the
  // contents are the message body.
  std::vector<uint8_t> bytes = CreateByteVector(message);
  SmartBuffer buf(bytes);
  EXPECT_TRUE(ContainsHttpBody(buf));
}

// Test that we can extract the IPP header from a message with an HTTP chunk
// header.
TEST(GetIppHeader, HttpChunkedHeader) {
  const std::string http_header =
      "Transfer-Encoding: chunked\x0d\x0a\x0d\x0a"
      // Chunk header.
      "8\x0d\x0a"
      // Chunk contents.
      "\x02\x00\x00\x06\x00\x00\x00\x05"s;

  std::vector<uint8_t> bytes = CreateByteVector(http_header);
  SmartBuffer buf(bytes);
  IppHeader ipp_header = GetIppHeader(buf);

  EXPECT_EQ(ipp_header.major, 2);
  EXPECT_EQ(ipp_header.minor, 0);
  EXPECT_EQ(ipp_header.operation_id, 0x0006);
  EXPECT_EQ(ipp_header.request_id, 5);
}

// Test that we can extract the IPP header from a message that has no HTTP
// header.
TEST(GetIppHeader, NoHttpHeader) {
  std::vector<uint8_t> message = {0x02, 0x00, 0x00, 0x06, 0x00, 0x00,
                                  0x00, 0x07, 0x00, 0x01, 0x70, 0x29};
  SmartBuffer buf(message);
  IppHeader ipp_header = GetIppHeader(buf);

  EXPECT_EQ(ipp_header.major, 2);
  EXPECT_EQ(ipp_header.minor, 0);
  EXPECT_EQ(ipp_header.operation_id, 0x0006);
  EXPECT_EQ(ipp_header.request_id, 7);
}

TEST(ExtractChunkSize, ValidChunk) {
  const std::string message =
      "1c\r\n"
      "hello world my name is david\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  ssize_t chunk_size = ExtractChunkSize(message_buffer);
  EXPECT_EQ(chunk_size, 0x1c);
}

TEST(ParseHttpChunkedMessage, MultipleChunks) {
  const std::string message =
      "4\r\n"
      "test\r\n"
      "5\r\n"
      "chunk\r\n"
      "0\r\n\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);

  SmartBuffer chunk1 = ParseHttpChunkedMessage(&message_buffer);
  const std::vector<uint8_t> expected1 = {'t', 'e', 's', 't'};
  EXPECT_EQ(chunk1.contents(), expected1);

  SmartBuffer chunk2 = ParseHttpChunkedMessage(&message_buffer);
  const std::vector<uint8_t> expected2 = {'c', 'h', 'u', 'n', 'k'};
  EXPECT_EQ(chunk2.contents(), expected2);

  SmartBuffer chunk3 = ParseHttpChunkedMessage(&message_buffer);
  EXPECT_EQ(chunk3.size(), 0);
  EXPECT_EQ(message_buffer.size(), 0);
}

TEST(ContainsFinalChunk, DoesContainFinalChunk) {
  const std::string message =
      "5\r\n"
      "hello\r\n"
      "0\r\n\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  EXPECT_TRUE(ContainsFinalChunk(message_buffer));
}

TEST(ContainsFinalChunk, NoFinalChunk) {
  const std::string message =
      "4\r\n"
      "test\r\n"
      "5\r\n"
      "chunk\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  EXPECT_FALSE(ContainsFinalChunk(message_buffer));
}

// Tests that ContainsFinalChunk will only return true when the final chunk
// (0\r\n\r\n) is found at the end of the message. In this case the final chunk
// is present as the message contents of one of the chunks, so it should not be
// treated as the final chunk.
TEST(ContainsFinalChunk, NotAtEnd) {
  const std::string message =
      "3\r\n"
      "0\r\n\r\n"
      "4\r\n"
      "test\r\n";
  SmartBuffer buf;
  buf.Add(message);
  EXPECT_FALSE(ContainsFinalChunk(buf));
}

TEST(ProcessMessageChunks, ContainsHttpHeader) {
  const std::string message =
      "Transfer-Encoding: chunked\r\n\r\n"
      "4\r\n"
      "test\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  // We expect ProcessMessageChunks to return true since |message| does not
  // contain the final 0-length chunk.
  EXPECT_TRUE(ProcessMessageChunks(&message_buffer));
  EXPECT_EQ(message_buffer.size(), 0);
}

TEST(ProcessMessageChunks, MultipleChunks) {
  const std::string message1 =
      "4\r\n"
      "test\r\n";
  SmartBuffer message_buffer1;
  message_buffer1.Add(message1);
  // We expect ProcessMessageChunks to return true since |message| does not
  // contain the final 0-length chunk.
  EXPECT_TRUE(ProcessMessageChunks(&message_buffer1));
  EXPECT_EQ(message_buffer1.size(), 0);

  const std::string message2 =
      "5\r\n"
      "chunk\r\n"
      "0\r\n\r\n";
  SmartBuffer message_buffer2;
  message_buffer2.Add(message2);
  // We expect ProcessMessageChunks to return false since |message| contains the
  // final 0-length chunk, and parsing should be completed.
  EXPECT_FALSE(ProcessMessageChunks(&message_buffer2));
  EXPECT_EQ(message_buffer2.size(), 0);
}

// Verify that GetHttpResponseHeader produces an HTTP header with the correct
// value set for the "Content-Length" field.
TEST(RemoveHttpHeader, ContainsHeader) {
  const std::string message = "POST /ipp/print HTTP/1.1\r\n\r\ntest";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  RemoveHttpHeader(&message_buffer);

  const std::vector<uint8_t> expected = {'t', 'e', 's', 't'};
  EXPECT_EQ(message_buffer.contents(), expected);
}

TEST(RemoveHttpHeader, NoHeader) {
  const std::string message = "no http header";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  EXPECT_DEATH(RemoveHttpHeader(&message_buffer),
               "Message does not contain HTTP header");
}

TEST(RemoveHttpHeader, InvalidHeader) {
  const std::string message =
      "POST /ipp/print HTTP/1.1 missing end of header indicator";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  EXPECT_DEATH(RemoveHttpHeader(&message_buffer),
               "Failed to find end of HTTP header");
}

TEST(MergeDocument, ValidMessage) {
  const std::string message =
      "6\r\n"
      "these \r\n"
      "7\r\n"
      "chunks \r\n"
      "7\r\n"
      "should \r\n"
      "5\r\n"
      "form \r\n"
      "14\r\n"
      "a complete sentence.\r\n"
      "0\r\n\r\n";
  SmartBuffer message_buffer;
  message_buffer.Add(message);

  const std::string expected_string =
      "these chunks should form a complete sentence.";
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer document = MergeDocument(&message_buffer);
  EXPECT_EQ(message_buffer.size(), 0);
  EXPECT_EQ(document.contents(), expected);
}

TEST(GetHttpResponseHeader, VerifyContentLength) {
  // The value we expect to be set for the "Content-Length" field in the
  // produced HTTP message.
  const int content_length = 1234009;
  const std::string expected =
    "HTTP/1.1 200 OK\r\n"
    "Server: localhost:0\r\n"
    "Content-Type: application/ipp\r\n"
    "Content-Length: 1234009\r\n"
    "Connection: close\r\n\r\n";
  EXPECT_EQ(GetHttpResponseHeader(content_length), expected);
}

TEST(GetIppTag, ValidTagName) {
  EXPECT_EQ(GetIppTag(kInteger), IPP_TAG_INTEGER);
  EXPECT_EQ(GetIppTag(kDateTime), IPP_TAG_DATE);
}

TEST(GetIppTag, InvalidTagName) {
  EXPECT_DEATH(GetIppTag("InvalidName"), "Given unknown tag name");
}

TEST(GetAttribute, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "printer-config-change-time",
      "value": 1834787
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  std::unique_ptr<base::Value> actual_value = GetJSONValue("1834787");
  IppAttribute expected("integer", "printer-config-change-time",
                        actual_value.get());
  EXPECT_EQ(GetAttribute(value.get()), expected);
}

TEST(GetAttribute, InvalidAttributes) {
  const std::string json_contents1 = "123";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  EXPECT_DEATH(GetAttribute(value1.get()), "Failed to retrieve dictionary");

  const std::string json_contents2 = R"(
    {
      "type": 123,
      "name": "printer-config-change-time",
      "value": ""
    }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  EXPECT_DEATH(GetAttribute(value2.get()), "Failed to retrieve type");

  const std::string json_contents3 = R"(
    {
      "type": "keyword",
      "name": [ "hello" ],
      "value": ""
    }
  )";
  std::unique_ptr<base::Value> value3 = GetJSONValue(json_contents3);
  EXPECT_DEATH(GetAttribute(value3.get()), "Failed to retrieve name");
}

TEST(GetAttributes, ValidAttributes) {
  const std::string operation_attributes_json = R"(
    {
      "operation_attributes": [{
        "type": "charset",
        "name": "attributes-charset",
        "value": "utf-8"
      }, {
        "type": "naturalLanguage",
        "name": "attributes-natural-language",
        "value": "en-us"
      }]
    }
  )";
  std::unique_ptr<base::Value> operation_attributes_value =
      GetJSONValue(operation_attributes_json);

  std::unique_ptr<base::Value> actual_value1 = GetJSONValue("\"utf-8\"");
  std::unique_ptr<base::Value> actual_value2 = GetJSONValue("\"en-us\"");
  std::vector<IppAttribute> expected = {
      IppAttribute("charset", "attributes-charset", actual_value1.get()),
      IppAttribute("naturalLanguage", "attributes-natural-language",
                   actual_value2.get())};

  std::vector<IppAttribute> actual =
      GetAttributes(operation_attributes_value.get(), "operation_attributes");

  EXPECT_EQ(actual, expected);
}

TEST(GetAttributes, InvalidAttributes) {
  // We expect this case to fail because the JSON value can't be interpreted as
  // a dictionary.
  const std::string json_contents1 = "123";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  EXPECT_DEATH(GetAttributes(value1.get(), ""),
               "Failed to retrieve dictionary");

  // We expect this case to fail because the there is no attributes list for the
  // provided key.
  const std::string json_contents2 = R"(
    {
      "printer_attributes": [
        { "type": "charset", "name": "charset-configured", "value": "utf-8" }
      ]
    }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  EXPECT_DEATH(GetAttributes(value2.get(), "operation_attributes"),
               "Failed to extract attributes list for key");

  const std::string json_contents3 = R"(
    { "printer_attributes": "not a list" }
  )";
  std::unique_ptr<base::Value> value3 = GetJSONValue(json_contents3);
  EXPECT_DEATH(GetAttributes(value3.get(), "printer_attributes"),
               "Failed to extract attributes list for key");
}

TEST(IppAttributeGetBool, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "boolean", "name": "color-supported", "value": false }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_EQ(attribute1.GetBool(), false);

  const std::string json_contents2 = R"(
    { "type": "boolean", "name": "color-supported", "value": true }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_EQ(attribute2.GetBool(), true);
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetBool, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "boolean", "name": "color-supported", "value": 0 }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_DEATH(attribute1.GetBool(), "Failed to retrieve boolean");

  const std::string json_contents2 = R"(
    { "type": "boolean", "name": "color-supported", "value": 0 }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_DEATH(attribute2.GetBool(), "Failed to retrieve boolean");
}

TEST(IppAttributeGetBools, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "something",
      "value": [ false, true, true, false ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  std::vector<bool> expected = {false, true, true, false};
  EXPECT_EQ(attribute.GetBools(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetBools, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "something",
      "value": [ false, true, true, false, "invalid" ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  EXPECT_DEATH(attribute.GetBools(), "Failed to retrieve boolean");
}

TEST(IppAttributeGetInt, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "integer", "name": "copies-default", "value": 1 }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_EQ(attribute1.GetInt(), 1);

  const std::string json_contents2 = R"(
    { "type": "integer", "name": "pages-per-minute", "value": 27 }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_EQ(attribute2.GetInt(), 27);
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetInt, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "integer", "name": "copies-default", "value": 1.33 }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_DEATH(attribute1.GetInt(), "Failed to retrieve integer");

  const std::string json_contents2 = R"(
    { "type": "integer", "name": "pages-per-minute", "value": "invalid" }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_DEATH(attribute2.GetInt(), "Failed to retrieve integer");
}

TEST(IppAttributeGetInts, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-bottom-margin-supported",
      "value": [ 511, 1023 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  std::vector<int> expected = {511, 1023};
  EXPECT_EQ(attribute.GetInts(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetInts, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-bottom-margin-supported",
      "value": [ 511, 1023, 1.33 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  EXPECT_DEATH(attribute.GetInts(), "Failed to retrieve integer");
}

TEST(IppAttributeGetString, ValidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "charset", "name": "attributes-charset", "value": "utf-8" }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_EQ(attribute1.GetString(), "utf-8");

  const std::string json_contents2 = R"(
    { "type": "keyword", "name": "compression-supported", "value": "none" }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_EQ(attribute2.GetString(), "none");
}

// In this test we expect crashes because the "value" fields in the attributes
// are invalid.
TEST(IppAttributeGetString, InvalidAttributes) {
  const std::string json_contents1 = R"(
    { "type": "charset", "name": "attributes-charset", "value": false }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_DEATH(attribute1.GetString(), "Failed to retrieve string");

  const std::string json_contents2 = R"(
    { "type": "keyword", "name": "compression-supported", "value": 123 }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_DEATH(attribute2.GetString(), "Failed to retrieve string");
}

TEST(IppAttributeGetStrings, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "which-jobs-supported",
      "value": [ "completed", "not-completed" ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  std::vector<std::string> expected = {"completed", "not-completed"};
  EXPECT_EQ(attribute.GetStrings(), expected);
}

// In this test we expect a crash because the "value" field in the attribute is
// invalid.
TEST(IppAttributeGetStrings, InvalidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "which-jobs-supported",
      "value": [ "completed", false ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  EXPECT_DEATH(attribute.GetStrings(), "Failed to retrieve string");
}

TEST(IppAttributeGetBytes, ValidAttributes) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  std::vector<uint8_t> expected = {7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0};
  EXPECT_EQ(attribute.GetBytes(), expected);
}

TEST(IppAttributeGetBytes, InvalidAttributes) {
  const std::string json_contents1 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 49, false, 18 ]
    }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  EXPECT_DEATH(attribute1.GetBytes(), "Failed to retrieve byte value");

  const std::string json_contents2 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 23, 1, -1 ]
    }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_DEATH(attribute2.GetBytes(), "Retrieved byte value is negative");

  const std::string json_contents3 = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 20, 15, 256 ]
    }
  )";
  std::unique_ptr<base::Value> value3 = GetJSONValue(json_contents3);
  IppAttribute attribute3 = GetAttribute(value3.get());
  EXPECT_DEATH(attribute3.GetBytes(), "Retrieved byte value is too large");
}

TEST(AddIppHeader, ValidHeader) {
  IppHeader header;
  header.major = 2;
  header.minor = 0;
  header.operation_id = 11;
  header.request_id = 3;
  std::vector<uint8_t> expected = {0x02, 0x00, 0x00, 0x0b,
                                   0x00, 0x00, 0x00, 0x03};
  SmartBuffer result(expected.size());
  AddIppHeader(header, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddPrinterAttributes, ValidAttributes) {
  const std::string json_contents = R"(
  {
    "operationAttributes": [{
      "type": "charset",
      "name": "attributes-charset",
      "value": "utf-8"
    }, {
      "type": "naturalLanguage",
      "name": "attributes-natural-language",
      "value": "en-us"
    }]
  })";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  std::vector<IppAttribute> attributes =
      GetAttributes(value.get(), kOperationAttributes);

  const std::string expected_string =
      "\x01"                         // Tag for operation-attributes.
      "\x47"                         // Type specifier for charset.
      "\x00\x12"                     // Length of name.
      "attributes-charset"           // Name.
      "\x00\x05"                     // Length of value.
      "utf-8"                        // Value.
      "\x48"                         // Type specifier for naturalLanguage.
      "\x00\x1b"                     // Length of name.
      "attributes-natural-language"  // Name.
      "\x00\x05"                     // Length of value.
      "en-us"s;                      // Value.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddPrinterAttributes(attributes, kOperationAttributes, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddBoolean, SingleValue) {
  const std::string json_contents = R"(
    { "type": "boolean", "name": "color-supported", "value": false }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x22"             // Type specifier for boolean.
      "\x00\x0f"         // Length of name.
      "color-supported"  // Name.
      "\x00\x01"         // Length of value.
      "\x00"s;           // Value.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddBoolean(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddBoolean, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "boolean",
      "name": "color-supported",
      "value": [ false, true, false, false ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x22"                  // Type specifier for boolean.
      "\x00\x0f"              // Length of name.
      "color-supported"       // Name.
      "\x00\x01"              // Length of value.
      "\x00"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x01"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x00"                  // Value.
      "\x22\x00\x00\x00\x01"  // Repeated value.
      "\x00"s;                // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddBoolean(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddInteger, SingleValue) {
  const std::string json_contents = R"(
    { "type": "integer", "name": "copies-default", "value": 1 }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x21"                // Type specifier for integer.
      "\x00\x0e"            // Length of name.
      "copies-default"      // Name.
      "\x00\x04"            // Length of value.
      "\x00\x00\x00\x01"s;  // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddInteger(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddInteger, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "integer",
      "name": "media-top-margin-supported",
      "value": [ 511, 1023 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x21"                        // Type specifier for integer.
      "\x00\x1a"                    // Length of name.
      "media-top-margin-supported"  // Name.
      "\x00\x04"                    // Length of value.
      "\x00\x00\x01\xff"            // Value.
      "\x21\x00\x00\x00\x04"        // Repeated value.
      "\x00\x00\x03\xff"s;          // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddInteger(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddString, SingleValue) {
  const std::string json_contents = R"(
    { "type": "keyword", "name": "compression-supported", "value": "none" }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x44"                   // Type specifier for keyword.
      "\x00\x15"               // Length of name.
      "compression-supported"  // Name.
      "\x00\x04"               // Length of value.
      "none"s;                 // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddString, MultipleValues) {
  const std::string json_contents = R"(
    {
      "type": "keyword",
      "name": "print-color-mode-supported",
      "value": [ "auto", "auto-monochrome", "monochrome" ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x44"                        // Type specifier for keyword.
      "\x00\x1a"                    // Length of name.
      "print-color-mode-supported"  // Name.
      "\x00\x04"                    // Length of value.
      "auto"                        // Value.
      "\x44\x00\x00\x00\x0f"        // Repeated value.
      "auto-monochrome"             // Value.
      "\x44\x00\x00\x00\x0a"        // Repeated value.
      "monochrome"s;                // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddDate, ValidDate) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": [ 7, 255, 8, 20, 15, 49, 49, 0, 45, 8, 0 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x31"                             // Type specifier for dateTime.
      "\x00\x1f"                         // Length of name.
      "printer-config-change-date-time"  // Name.
      "\x00\x0b"                         // Length of value.
      "\x07\xff\x08\x14\x0f\x31\x31\x00\x2d\x08\x00"s;  // Value.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddDate(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

// We expect this test to fail because the "value" field for the "dateTime"
// attribute is not a list.
TEST(AddDate, InvalidDate) {
  const std::string json_contents = R"(
    {
      "type": "dateTime",
      "name": "printer-config-change-date-time",
      "value": "not a list"
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  SmartBuffer result(0);
  EXPECT_DEATH(AddDate(attribute, &result),
               "Date value is in an incorrect format");
}

TEST(AddOctetString, StringValue) {
  const std::string json_contents = R"(
    {
      "type": "octetString",
      "name": "printer-input-tray",
      "value": "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2"
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x30"                // Type specifier for octetString.
      "\x00\x12"            // Length of name.
      "printer-input-tray"  // Name.
      "\x00\x3c"            // Length of value.
      // Value
      "type=other;mediafeed=0;mediaxfeed=0;maxcapacity=250;level=-2"s;
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddOctetString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddOctetString, BytesValue) {
  const std::string json_contents = R"(
    {
      "type": "octetString",
      "name": "printer-firmware-version",
      "value": [0, 4, 15, 6]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x30"                      // Type specifier for octetString.
      "\x00\x18"                  // Length of name.
      "printer-firmware-version"  // Name.
      "\x00\x04"                  // Length of array.
      "\x00\x04\x0f\x06"s;        // Array elements.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddOctetString(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddRange, ValidRange) {
  const std::string json_contents = R"(
    {
      "type": "rangeOfInteger",
      "name": "copies-supported",
      "value": [ 1, 1023 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x33"                // Type specifier for range.
      "\x00\x10"            // Length of name.
      "copies-supported"    // Name.
      "\x00\x08"            // Length of range.
      "\x00\x00\x00\x01"    // Range lower bound.
      "\x00\x00\x03\xff"s;  // Range upper bound.
  std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddRange(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

// We expect this case to fail because the "value" field for the
// "rangeOfInteger" field is not a list.
TEST(AddRange, InvalidRange) {
  const std::string json_contents = R"(
    {
      "type": "rangeOfInteger",
      "name": "copies-supported",
      "value": "1-1023"
    }
  )";
  SmartBuffer result(0);
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());
  EXPECT_DEATH(AddRange(attribute, &result),
               "Range value is in an incorrect format");
}

TEST(AddResolution, ValidResolution) {
  const std::string json_contents = R"(
    {
      "type": "resolution",
      "name": "printer-resolution-default",
      "value": [ 300, 300, 3 ]
    }
  )";
  std::unique_ptr<base::Value> value = GetJSONValue(json_contents);
  IppAttribute attribute = GetAttribute(value.get());

  const std::string expected_string =
      "\x32"                        // Type specifier for resolution.
      "\x00\x1a"                    // Length of name.
      "printer-resolution-default"  // Name.
      "\x00\x09"                    // Length of resolution value.
      "\x00\x00\x01\x2c"            // Cross-feed (X) resolution size.
      "\x00\x00\x01\x2c"            // Feed (Y) resolution size.
      "\x03"s;                      // Unit type.
  const std::vector<uint8_t> expected = CreateByteVector(expected_string);

  SmartBuffer result(expected.size());
  AddResolution(attribute, &result);
  EXPECT_EQ(expected, result.contents());
}

TEST(AddResolution, InvalidResolution) {
  // We expect this case to fail because the "value" field for the "resolution"
  // attribute is not a list.
  const std::string json_contents1 = R"(
    {
      "type": "resolution",
      "name": "copies-supported",
      "value": "300, 300, 3"
    }
  )";
  std::unique_ptr<base::Value> value1 = GetJSONValue(json_contents1);
  IppAttribute attribute1 = GetAttribute(value1.get());
  SmartBuffer result(0);
  EXPECT_DEATH(AddResolution(attribute1, &result),
               "Resolution value is in an incorrect format");

  // We expect this case to fail because the "value" field for the "resolution"
  // attribute is not a list.
  const std::string json_contents2 = R"(
    {
      "type": "resolution",
      "name": "copies-supported",
      "value": [ 300, 300, 3, 4 ]
    }
  )";
  std::unique_ptr<base::Value> value2 = GetJSONValue(json_contents2);
  IppAttribute attribute2 = GetAttribute(value2.get());
  EXPECT_DEATH(AddResolution(attribute2, &result),
               "Resolution list is an invalid size");
}

}  // namespace
