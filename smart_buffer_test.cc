// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smart_buffer.h"

#include "device_descriptors.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(Add, AddPrimitive) {
  SmartBuffer buf1(1);
  SmartBuffer buf2(1);
  std::vector<uint8_t> expected = {'h'};
  uint8_t byte = 'h';

  buf1.Add(&byte, sizeof(byte));
  EXPECT_EQ(expected, buf1.contents());
  buf2.Add(byte);
  EXPECT_EQ(expected, buf2.contents());
}

TEST(Add, AddVector) {
  SmartBuffer buf1(5);
  SmartBuffer buf2(5);
  std::vector<uint8_t> expected = {1, 2, 3, 4, 5};

  buf1.Add(expected.data(), expected.size());
  EXPECT_EQ(expected, buf1.contents());
  buf2.Add(expected);
  EXPECT_EQ(expected, buf2.contents());
}

TEST(Add, AddString) {
  SmartBuffer buf1(10);
  SmartBuffer buf2(10);
  std::string s = "helloworld";
  std::vector<uint8_t> expected = {'h', 'e', 'l', 'l', 'o',
                                   'w', 'o', 'r', 'l', 'd'};
  buf1.Add(s.c_str(), s.size());
  EXPECT_EQ(expected, buf1.contents());
  buf2.Add(s);
  EXPECT_EQ(expected, buf2.contents());
}

TEST(Add, AddDeviceDescriptor) {
  UsbDeviceDescriptor dev_dsc(18, 1, 272, 0, 0, 0, 8, 1193, 10216, 0, 1, 2, 1,
                              1);
  std::vector<uint8_t> expected = {18, 1, 16, 1, 0, 0, 0, 8,
                                   169, 4, 232, 39, 0, 0, 1, 2, 1, 1};
  SmartBuffer buf1(sizeof(dev_dsc));
  buf1.Add(&dev_dsc, sizeof(dev_dsc));
  EXPECT_EQ(expected, buf1.contents());

  SmartBuffer buf2(sizeof(dev_dsc));
  buf2.Add(dev_dsc);
  EXPECT_EQ(expected, buf2.contents());
}

TEST(Add, AddSmartBuffer) {
  SmartBuffer buf1(5);
  SmartBuffer buf2(5);
  std::vector<uint8_t> expected = {1, 2, 3, 4, 5};
  buf1.Add(expected.data(), expected.size());
  buf2.Add(buf1);
  EXPECT_EQ(buf1.contents(), buf2.contents());
}

// Test that the proper suffix from the provided SmartBuffer is added.
TEST(Add, AddSmartBufferSuffix) {
  SmartBuffer buf1(5);
  SmartBuffer buf2(5);
  std::vector<uint8_t> contents = {1, 2, 3, 4, 5};
  buf1.Add(contents.data(), contents.size());

  std::vector<uint8_t> expected = {3, 4, 5};
  buf2.Add(buf1, 2);
  EXPECT_EQ(expected, buf2.contents());
}

// Test that if the provided start point is 0, then the entire contents of the
// provided SmartBuffer is added.
TEST(Add, AddSmartBufferFullSuffix) {
  SmartBuffer buf1(5);
  SmartBuffer buf2(5);
  std::vector<uint8_t> contents = {1, 2, 3, 4, 5};
  buf1.Add(contents.data(), contents.size());

  buf2.Add(buf1, 0);
  EXPECT_EQ(contents, buf2.contents());
}

TEST(Add, AddSmartBufferRange) {
  SmartBuffer to_copy({1, 2, 3, 4, 5});
  SmartBuffer to_extend(5);
  to_extend.Add(to_copy, 1, 3);
  const std::vector<uint8_t> expected = {2, 3, 4};
  EXPECT_EQ(to_extend.contents(), expected);
}

TEST(Erase, EraseSmartBufferRange) {
  SmartBuffer buf({1, 2, 3, 4, 5});
  const std::vector<uint8_t> expected = {1, 5};
  buf.Erase(1, 3);
  EXPECT_EQ(buf.contents(), expected);
}

TEST(Resize, Shrink) {
  SmartBuffer buf(5);
  std::vector<uint8_t> contents = {1, 2, 3, 4, 5};
  buf.Add(contents.data(), contents.size());
  buf.Shrink(3);
  std::vector<uint8_t> expected = {1, 2, 3};
  EXPECT_EQ(expected, buf.contents());
}

}  // namespace
