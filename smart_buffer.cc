// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smart_buffer.h"

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>

SmartBuffer::SmartBuffer(size_t size) {
  // Set the capacity of |buffer_| to |size|.
  buffer_.reserve(size);
}

SmartBuffer::SmartBuffer(const std::vector<uint8_t>& v) : buffer_(v) {}

void SmartBuffer::Add(const std::string& s) {
  Add(s.c_str(), s.size());
}

// Adds the contents from |buf|.
void SmartBuffer::Add(const SmartBuffer& buf) {
  const std::vector<uint8_t> contents = buf.contents();
  buffer_.insert(buffer_.end(), contents.begin(), contents.end());
}

// Add the contents from |buf|, starting from |start|.
void SmartBuffer::Add(const SmartBuffer& buf, size_t start) {
  const std::vector<uint8_t>& contents = buf.contents();
  buffer_.insert(buffer_.end(), contents.begin() + start, contents.end());
}

void SmartBuffer::Add(const SmartBuffer& buf, size_t start, size_t len) {
  CHECK_LE(start + len, buf.size()) << "Given range out of bounds";
  const std::vector<uint8_t>& contents = buf.contents();
  buffer_.insert(buffer_.end(), contents.begin() + start,
                 contents.begin() + start + len);
}

void SmartBuffer::Erase(size_t index) {
  buffer_.erase(buffer_.begin() + index);
}

void SmartBuffer::Erase(size_t start, size_t len) {
  buffer_.erase(buffer_.begin() + start, buffer_.begin() + start + len);
}

void SmartBuffer::Shrink(size_t size) {
  if (size >= buffer_.size()) {
    LOG(INFO) << "Can't shrink to a size larger than current buffer";
    return;
  }
  buffer_.resize(size);
}

ssize_t SmartBuffer::FindFirstOccurrence(const std::string& target,
                                         size_t start) const {
  const std::vector<uint8_t>& contents = this->contents();
  auto iter = std::search(contents.begin() + start, contents.end(),
                          target.begin(), target.end());
  if (iter == contents.end()) {
    return -1;
  }
  return std::distance(contents.begin(), iter);
}
