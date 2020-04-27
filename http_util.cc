// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "http_util.h"

#include <vector>

namespace {

// Determines if |message| starts with the string |target|.
bool StartsWith(const SmartBuffer& message, const std::string& target) {
  return message.FindFirstOccurrence(target) == 0;
}

bool MessageContains(const SmartBuffer& message, const std::string& s) {
  ssize_t pos = message.FindFirstOccurrence(s);
  return pos != -1;
}

}  // namespace

bool IsHttpChunkedMessage(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("Transfer-Encoding: chunked");
  return i != -1;
}

bool ContainsHttpHeader(const SmartBuffer& message) {
  return MessageContains(message, "POST /ipp/print HTTP");
}

bool ContainsHttpBody(const SmartBuffer& message) {
  // We are making the assumption that if |message| does not contain an HTTP
  // header then |message| is the body of an HTTP message.
  if (!ContainsHttpHeader(message)) {
    return true;
  }
  // If |message| contains an HTTP header, check to see if there's anything
  // immediately following it.
  ssize_t pos = message.FindFirstOccurrence("\r\n\r\n");
  CHECK_GE(pos, 0) << "HTTP header does not contain end-of-header marker";
  size_t start = pos + 4;
  return start < message.size();
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
  return std::stoll(hex_string, 0, 16);
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
  return i > 0 && message.size() == i + 5;
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

void RemoveHttpHeader(SmartBuffer* message) {
  CHECK(ContainsHttpHeader(*message)) << "Message does not contain HTTP header";
  int i = message->FindFirstOccurrence("\r\n\r\n");
  CHECK_GT(i, 0) << "Failed to find end of HTTP header";
  message->Erase(0, i + 4);
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

std::string GetHttpResponseHeader(size_t size) {
  return "HTTP/1.1 200 OK\r\n"
         "Server: localhost:0\r\n"
         "Content-Type: application/ipp\r\n"
         "Content-Length: " +
         std::to_string(size) +
         "\r\n"
         "Connection: close\r\n\r\n";
}
