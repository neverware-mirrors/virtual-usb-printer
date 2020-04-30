// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __VALUD_UTIL_H__
#define __VALUD_UTIL_H__

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
std::unique_ptr<base::Value> GetJSONValue(const std::string& json_contents);

// Utility function for converting |value| into a DictionaryValue object.
const base::DictionaryValue* GetDictionary(const base::Value* value);

#endif  // __VALUD_UTIL_H__
