// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __HTTP_UTIL_H__
#define __HTTP_UTIL_H__

#include <string>

#include "smart_buffer.h"

bool IsHttpChunkedMessage(const SmartBuffer& message);

bool ContainsHttpHeader(const SmartBuffer& message);

// Determines if |message| contains the body of an HTTP message.
bool ContainsHttpBody(const SmartBuffer& message);

size_t ExtractChunkSize(const SmartBuffer& message);

SmartBuffer ParseHttpChunkedMessage(SmartBuffer* message);

// Checks if |message| contains the terminating chunk.
bool ContainsFinalChunk(const SmartBuffer& message);

// Extracts each of the messages chunks from |message|. Returns true if the
// final "0-length" chunk has not been processed and there are still more chunks
// to be received.
bool ProcessMessageChunks(SmartBuffer* message);

// Removes the HTTP header from |message|.
void RemoveHttpHeader(SmartBuffer* message);

// Extracts the IPP message from the first HTTP chunked message in |message|.
// This function assumes that the first chunk in |message| contains the IPP
// message.
SmartBuffer ExtractIppMessage(SmartBuffer* message);

// Merge the HTTP chunked messages from |message| into a single SmartBuffer. It
// is assumed that |message| only contains the chunks which make up the received
// document file.
SmartBuffer MergeDocument(SmartBuffer* message);

// Create a generic HTTP response header with the "Content-Length" field set to
// |size|.
std::string GetHttpResponseHeader(size_t size);

#endif  // __HTTP_UTIL_H__
