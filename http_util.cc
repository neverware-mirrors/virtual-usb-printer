// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "http_util.h"

#include <algorithm>
#include <vector>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {

const std::string kHttpRequestEnd = "\r\n\r\n";  // NOLINT(runtime/string)
const std::string kHttpLineEnd = "\r\n";         // NOLINT(runtime/string)

// Determines if |message| starts with the string |target|.
bool StartsWith(const SmartBuffer& message, const std::string& target) {
  return message.FindFirstOccurrence(target) == 0;
}

base::Optional<HttpHeaders> ParseHttpHeaders(const std::string& headers) {
  std::vector<std::string> lines = base::SplitStringUsingSubstr(
      headers, kHttpLineEnd, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  const std::string header_split = ":";
  HttpHeaders parsed_headers;
  for (const std::string& line : lines) {
    size_t split = line.find(header_split);
    if (split == std::string::npos) {
      LOG(ERROR) << "Malformed header: '" << line << "'";
      return base::nullopt;
    }

    parsed_headers.emplace(
        line.substr(0, split),
        base::TrimString(line.substr(split + header_split.size()), " ",
                         base::TRIM_LEADING));
  }
  return parsed_headers;
}

bool ValidateHttpVersion(const std::string& version) {
  std::vector<std::string> tokens = base::SplitString(
      version, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 2 || tokens[0] != "HTTP") {
    LOG(ERROR) << "Malformed HTTP version: '" << version << "'";
    return false;
  }

  // 1.0 support is prohibited by IPP spec for HTTP transport.
  if (tokens[1] == "1.0") {
    LOG(ERROR) << "HTTP version 1.0 is not supported";
    return false;
  }
  return true;
}

}  // namespace

// static
base::Optional<HttpRequest> HttpRequest::Deserialize(SmartBuffer* message) {
  ssize_t request_end = message->FindFirstOccurrence(kHttpRequestEnd);
  if (request_end == -1) {
    LOG(ERROR) << "Message does not contain end of header marker";
    return base::nullopt;
  }

  ssize_t request_line_end = message->FindFirstOccurrence(kHttpLineEnd);
  if (request_line_end == -1) {
    LOG(ERROR) << "Message does not contain end of line marker";
    return base::nullopt;
  }

  if (request_line_end == 0) {
    LOG(ERROR) << "Request line is empty";
    return base::nullopt;
  }

  // First parse the first line of the request, which should look something like
  // "GET /ipp/print HTTP/1.1" or "POST /eSCL/ScannerCapabilities HTTP/1.1".
  std::string request_line(message->data(), message->data() + request_line_end);
  std::vector<std::string> request_line_tokens = base::SplitString(
      request_line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (request_line_tokens.size() != 3) {
    LOG(ERROR) << "Malformed request line: '" << request_line << "'";
    return base::nullopt;
  }

  std::string http_version = request_line_tokens[2];
  if (!ValidateHttpVersion(http_version))
    return base::nullopt;

  HttpRequest request;
  request.method = request_line_tokens[0];
  request.uri = request_line_tokens[1];

  // Now, parse the rest of the HTTP request after the request line as headers.

  // In case that there are no headers, make sure when we skip the kHttpLineEnd
  // we aren't actually skipping past the kHttpRequestEnd marker.
  size_t headers_start =
      std::min<size_t>(request_line_end + kHttpLineEnd.size(), request_end);
  std::string request_headers(message->data() + headers_start,
                              message->data() + request_end);
  base::Optional<HttpHeaders> headers = ParseHttpHeaders(request_headers);
  if (!headers) {
    LOG(ERROR) << "Failed to parse request headers";
    return base::nullopt;
  }
  request.headers = headers.value();

  // Erase the data we just parsed from |message|.
  message->Erase(0, request_end + kHttpRequestEnd.size());
  return request;
}

size_t HttpRequest::ContentLength() const {
  const auto content_length = headers.find("Content-Length");
  if (content_length == headers.end())
    return 0;

  size_t length;
  if (!base::StringToSizeT(content_length->second, &length)) {
    LOG(ERROR) << "Could not convert Content-Length header to integer: "
               << content_length->second;
    return 0;
  }

  return length;
}

bool HttpRequest::IsChunkedMessage() const {
  auto encoding = headers.find("Transfer-Encoding");
  return encoding != headers.end() && encoding->second == "chunked";
}

void HttpResponse::Serialize(SmartBuffer* buf) const {
  buf->Add("HTTP/1.1 ");
  buf->Add(status);
  buf->Add("\r\n");

  HttpHeaders headers_copy = headers;
  // Add standard headers
  headers_copy["Server"] = "localhost:0";
  headers_copy["Connection"] = "close";
  headers_copy["Content-Length"] = std::to_string(body.size());
  for (auto header : headers_copy) {
    buf->Add(header.first);
    buf->Add(": ");
    buf->Add(header.second);
    buf->Add("\r\n");
  }
  buf->Add("\r\n");
  buf->Add(body);
}

bool IsHttpChunkedMessage(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("Transfer-Encoding: chunked");
  return i != -1;
}

size_t ExtractChunkSize(const SmartBuffer& message) {
  ssize_t end = message.FindFirstOccurrence("\r\n");
  if (end == -1) {
    return 0;
  }
  std::string hex_string;
  const std::vector<uint8_t>& contents = message.contents();
  for (size_t i = 0; i < end; ++i) {
    hex_string += static_cast<char>(contents[i]);
  }

  uint32_t chunk_size = 0;
  if (!base::HexStringToUInt(hex_string, &chunk_size)) {
    LOG(ERROR) << "Could not parse '" << hex_string << "' as hex.";
    LOG(ERROR) << "Further errors may occur due to incorrect chunk reading.";
    return 0;
  }

  return chunk_size;
}

SmartBuffer ParseHttpChunkedMessage(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Given null message";
  // If |message| starts with the trailing CRLF end-of-chunk indicator from the
  // previous chunk then erase it.
  if (StartsWith(*message, "\r\n")) {
    message->Erase(0, 2);
  }
  size_t chunk_size = ExtractChunkSize(*message);
  LOG(INFO) << "Chunk size: " << chunk_size;
  ssize_t start = message->FindFirstOccurrence("\r\n");
  SmartBuffer ret(0);
  if (start == -1) {
    return ret;
  }

  ret.Add(*message, start + 2, chunk_size);
  // In case |message| contains multiple chunks, we remove the chunk which was
  // just parsed.
  //
  // The length of the prefix to be deleted is calculated as follows:
  // start      - represents the hex-encoded length value.
  // chunk_size - the number of bytes read out for the message body.
  // 2          - the CRLF characters which trail the length.
  size_t to_erase_length = start + chunk_size + 2;
  CHECK_GE(message->size(), to_erase_length);
  message->Erase(0, to_erase_length);
  // If |message| also contains the trailing CRLF end-of-chunk indicator, then
  // erase it.
  if (StartsWith(*message, "\r\n")) {
    message->Erase(0, 2);
  }

  return ret;
}

bool ContainsFinalChunk(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("0\r\n\r\n");
  return i >= 0 && message.size() == i + 5;
}

bool ProcessMessageChunks(SmartBuffer* message) {
  CHECK(message != nullptr) << "Process - Given null message";
  if (IsHttpChunkedMessage(*message)) {
    // If |message| contains an HTTP header then we discard it.
    int start = message->FindFirstOccurrence("\r\n\r\n");
    CHECK_GT(start, 0) << "Failed to process HTTP chunked message";
    message->Erase(0, start + 4);
  }

  SmartBuffer chunk;
  while (message->size() > 0) {
    chunk = ParseHttpChunkedMessage(message);
  }
  return chunk.size() != 0;
}

SmartBuffer ExtractIppMessage(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Received null message";
  return ParseHttpChunkedMessage(message);
}

SmartBuffer MergeDocument(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Received null message";
  SmartBuffer document;
  while (message->size() > 0) {
    SmartBuffer chunk = ParseHttpChunkedMessage(message);
    document.Add(chunk);
  }
  return document;
}
