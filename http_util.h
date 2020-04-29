// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __HTTP_UTIL_H__
#define __HTTP_UTIL_H__

#include <string>

#include <base/containers/flat_map.h>
#include <base/optional.h>

#include "smart_buffer.h"

using HttpHeaders = base::flat_map<std::string, std::string>;
class HttpRequest {
 public:
  // Attempts to parse an HttpRequest from the beginning of |message|.
  // If successful, remove the header from |message|, so that it contains only
  // the body of the http request.
  static base::Optional<HttpRequest> Deserialize(SmartBuffer* message);

  // Returns true if this header contains a request header indicating a chunked
  // transfer encoding.
  bool IsChunkedMessage() const;

  std::string method;
  std::string uri;
  HttpHeaders headers;
};

class HttpResponse {
 public:
  std::string status;
  HttpHeaders headers;
  SmartBuffer body;

  // Serializes this HttpResponse to the textual format specified by the HTTP
  // standard and appends it to the contents of |buf|.
  void Serialize(SmartBuffer* buf) const;
};

bool IsHttpChunkedMessage(const SmartBuffer& message);

size_t ExtractChunkSize(const SmartBuffer& message);

SmartBuffer ParseHttpChunkedMessage(SmartBuffer* message);

// Checks if |message| contains the terminating chunk.
bool ContainsFinalChunk(const SmartBuffer& message);

// Extracts each of the messages chunks from |message|. Returns true if the
// final "0-length" chunk has not been processed and there are still more chunks
// to be received.
bool ProcessMessageChunks(SmartBuffer* message);

// Extracts the IPP message from the first HTTP chunked message in |message|.
// This function assumes that the first chunk in |message| contains the IPP
// message.
SmartBuffer ExtractIppMessage(SmartBuffer* message);

// Merge the HTTP chunked messages from |message| into a single SmartBuffer. It
// is assumed that |message| only contains the chunks which make up the received
// document file.
SmartBuffer MergeDocument(SmartBuffer* message);

#endif  // __HTTP_UTIL_H__
