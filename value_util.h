// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VALUE_UTIL_H__
#define VALUE_UTIL_H__

#include <memory>
#include <string>

#include <base/json/json_reader.h>
#include <base/optional.h>
#include <base/values.h>

// Attempt to load the contents of the JSON file located at |file_path| and
// return the contents in a string.
base::Optional<std::string> GetJSONContents(const std::string& file_path);

// Use a JSONReader to parse |json_contents| and return a pointer to the
// underlying Value object.
base::Optional<base::Value> GetJSONValue(const std::string& json_contents);

#endif  // VALUE_UTIL_H__
