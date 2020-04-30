// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "http_util.h"

#include <string>
#include <vector>

#include <base/containers/flat_map.h>
#include <gtest/gtest.h>

using std::string_literals::operator""s;

namespace {

const std::vector<uint8_t> CreateByteVector(const std::string& str) {
  std::vector<uint8_t> v(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    v[i] = static_cast<uint8_t>(str[i]);
  }
  return v;
}

}  // namespace

TEST(HttpRequest, DeserializeNoHeaders) {
  const std::string http_request = "GET /eSCL/ScannerStatus HTTP/1.1\r\n\r\n";

  SmartBuffer buf;
  buf.Add(http_request);
  base::Optional<HttpRequest> opt_request = HttpRequest::Deserialize(&buf);
  EXPECT_TRUE(opt_request.has_value());
  HttpRequest request = opt_request.value();
  EXPECT_EQ(request.method, "GET");
  EXPECT_EQ(request.uri, "/eSCL/ScannerStatus");

  EXPECT_EQ(request.headers.size(), 0);
  EXPECT_FALSE(request.IsChunkedMessage());
}

TEST(HttpRequest, DeserializeChunked) {
  const std::string http_request =
      "POST /ipp/print HTTP/1.1\r\n"
      "Content-Type: application/ipp\r\n"
      "Date: Mon, 12 Nov 2018 19:17:31 GMT\r\n"
      "Host: localhost:0\r\n"
      "Transfer-Encoding: chunked\r\n"
      "User-Agent: CUPS/2.2.8 (Linux 4.14.82; x86_64) IPP/2.0\r\n"
      "Expect: 100-continue\r\n\r\n"
      "request body";

  SmartBuffer buf;
  buf.Add(http_request);

  base::Optional<HttpRequest> opt_request = HttpRequest::Deserialize(&buf);
  EXPECT_TRUE(opt_request.has_value());
  HttpRequest request = opt_request.value();
  EXPECT_EQ(request.method, "POST");
  EXPECT_EQ(request.uri, "/ipp/print");

  HttpHeaders expected_headers{
      {"Content-Type", "application/ipp"},
      {"Date", "Mon, 12 Nov 2018 19:17:31 GMT"},
      {"Host", "localhost:0"},
      {"Transfer-Encoding", "chunked"},
      {"User-Agent", "CUPS/2.2.8 (Linux 4.14.82; x86_64) IPP/2.0"},
      {"Expect", "100-continue"},
  };
  EXPECT_EQ(request.headers, expected_headers);
  EXPECT_TRUE(request.IsChunkedMessage());
  EXPECT_EQ(buf.contents(), CreateByteVector("request body"));
}

TEST(HttpRequest, MalformedHeader) {
  const std::string http_request =
      "POST /ipp/print HTTP/1.1\r\n"
      "Content-Type application/ipp\r\n\r\n";

  SmartBuffer buf;
  buf.Add(http_request);
  EXPECT_FALSE(HttpRequest::Deserialize(&buf).has_value());
}

TEST(HttpRequest, MalformedRequestLine) {
  const std::string http_request =
      "GET /ipp/print HTTP1.1\r"
      "Content-Type: application/ipp\r\n\r\n";

  SmartBuffer buf;
  buf.Add(http_request);
  EXPECT_FALSE(HttpRequest::Deserialize(&buf).has_value());
}

TEST(HttpRequest, NoEndOfHeaderMarker) {
  const std::string http_request =
      "GET /ipp/print HTTP/1.1\r\n"
      "Content-Type: application/ipp\r\n";

  SmartBuffer buf;
  buf.Add(http_request);
  EXPECT_FALSE(HttpRequest::Deserialize(&buf).has_value());
}

TEST(HttpResponse, Serialize) {
  HttpResponse response;
  response.status = "200 OK";
  response.headers["Test"] = "Header";
  response.body.Add("[body]");

  SmartBuffer serialized;
  response.Serialize(&serialized);

  const std::string expected_response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Length: 6\r\n"
      "Server: localhost:0\r\n"
      "Test: Header\r\n\r\n"
      "[body]"s;

  const std::string actual_response(serialized.contents().begin(),
                                    serialized.contents().end());
  EXPECT_EQ(actual_response, expected_response);
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
  SmartBuffer original_message_buffer = message_buffer;
  EXPECT_FALSE(RemoveHttpHeader(&message_buffer));
  EXPECT_EQ(message_buffer.contents(), original_message_buffer.contents());
}

TEST(RemoveHttpHeader, InvalidHeader) {
  const std::string message =
      "POST /ipp/print HTTP/1.1 missing end of header indicator";
  SmartBuffer message_buffer;
  message_buffer.Add(message);
  SmartBuffer original_message_buffer = message_buffer;
  EXPECT_FALSE(RemoveHttpHeader(&message_buffer));
  EXPECT_EQ(message_buffer.contents(), original_message_buffer.contents());
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
