// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "value_util.h"

#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/optional.h>
#include <base/values.h>

base::Optional<std::string> GetJSONContents(const std::string& file_path) {
  std::string json_contents;
  const base::FilePath path(file_path);
  if (!base::ReadFileToString(path, &json_contents)) {
    return {};
  }
  return json_contents;
}

base::Optional<base::Value> GetJSONValue(const std::string& json_contents) {
  base::Optional<base::Value> value = base::JSONReader::Read(json_contents);
  CHECK(value) << "Failed to parse JSON string";
  return value;
}
