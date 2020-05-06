// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __SMART_BUFFER_H__
#define __SMART_BUFFER_H__

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>

// Wrapper class used for packing bytes to be transferred on a network socket.
class SmartBuffer {
 public:
  SmartBuffer() = default;

  // Initialize the buffer with an initial size of |size|.
  explicit SmartBuffer(size_t size);

  // Initialize the buffer with the same contents as |v|.
  explicit SmartBuffer(const std::vector<uint8_t>& v);

  // Convert |data| into uint8_t* and add |data_size| bytes to the internal
  // buffer.
  template <typename T>
  void Add(const T* data, size_t data_size);

  // Add the underlying byte representation of |data| to the buffer.
  template <typename T>
  void Add(const T& data);

  // Specialized Add method for std::vector so that we can call |v.data()| and
  // |v.size()| to extract the underlying pointer and size of the data.
  template <typename T>
  void Add(const std::vector<T>& v);

  // Specialized Add method for std::string so that we can call |s.c_str()| and
  // |s.size()| to extract the underlying pointer and size of the data.
  void Add(const std::string& s);

  // Specialized Add method for char* that uses strlen.
  void Add(const char* s);

  // Adds the contents from |buf|.
  void Add(const SmartBuffer& buf);

  // Add the contents from |buf|, starting from |start|.
  void Add(const SmartBuffer& buf, size_t start);

  // Add the subsequence of |buf| starting at |start| and of length |len|.
  void Add(const SmartBuffer& buf, size_t start, size_t len);

  // Erases the element at |index|.
  void Erase(size_t index);

  // Erases the subsequence of length |len| starting at |start| from |buffer|.
  void Erase(size_t start, size_t len);

  // Shrink the underlying vector to |size|.
  void Shrink(size_t size);

  // Find first occurrence of |target| in the buffer and return the index to
  // where it begins. If |target| does not appear in |message| then returns -1.
  ssize_t FindFirstOccurrence(const std::string& target,
                              size_t start = 0) const;

  size_t size() const { return buffer_.size(); }
  const std::vector<uint8_t>& contents() const { return buffer_; }
  const uint8_t* data() const { return buffer_.data(); }

 private:
  std::vector<uint8_t> buffer_;
};

template <typename T>
void SmartBuffer::Add(const T* data, size_t data_size) {
  const uint8_t* packed_data = reinterpret_cast<const uint8_t*>(data);
  buffer_.insert(buffer_.end(), packed_data, packed_data + data_size);
}

template <typename T>
void SmartBuffer::Add(const T& data) {
  static_assert(!std::is_pointer<T>::value,
                "cannot use Add(const T&) for pointer types. If you want to "
                "Add a pointer type, use Add(const T* data, size)");
  Add(&data, sizeof(data));
}

template <typename T>
void SmartBuffer::Add(const std::vector<T>& v) {
  CHECK(std::is_fundamental<T>::value)
      << "Given vector does not contain fundamental elements";
  size_t byte_size = sizeof(T) * v.size();
  Add(v.data(), byte_size);
}

#endif  // __SMART_BUFFER_H__
