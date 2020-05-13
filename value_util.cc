// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "value_util.h"

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

std::unique_ptr<base::Value> GetJSONValue(const std::string& json_contents) {
  base::JSONReader json_reader;
  // TODO(crbug/1054279): migrate to base::JSONReader::Read after uprev.
  std::unique_ptr<base::Value> value =
      json_reader.ReadDeprecated(json_contents);
  CHECK(value) << "Failed to parse JSON string";
  return value;
}

const base::DictionaryValue* GetDictionary(const base::Value* value) {
  const base::DictionaryValue* dict;
  CHECK(value->GetAsDictionary(&dict)) << "Failed to extract value of type "
                                       << value->type() << " as dictionary";
  return dict;
}
