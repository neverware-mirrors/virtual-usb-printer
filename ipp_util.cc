// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_util.h"

#include "smart_buffer.h"
#include "usbip_constants.h"

#include <map>
#include <set>
#include <string>

#include <base/values.h>
#include <cups/cups.h>
#include <cups/ipp.h>

namespace {

std::vector<IppAttribute> GetAttributes(const base::ListValue* attributes) {
  std::vector<IppAttribute> ipp_attributes;
  for (std::size_t i = 0; i < attributes->GetSize(); ++i) {
    const base::DictionaryValue* attributes_dictionary;
    if (!attributes->GetDictionary(i, &attributes_dictionary)) {
      LOG(ERROR)
          << "Failed to extract dictionary value from attributes list at index "
          << i;
    }
    ipp_attributes.push_back(GetAttribute(attributes_dictionary));
  }
  return ipp_attributes;
}

// Find first occurrence of |target| in |message| and return the index to where
// it begins. If |target| does not appear in |message| then returns -1.
ssize_t FindFirstOccurrence(const SmartBuffer& message,
                            const std::string& target, size_t start = 0) {
  const std::vector<uint8_t>& contents = message.contents();
  auto iter = std::search(contents.begin() + start, contents.end(),
                          target.begin(), target.end());
  if (iter == contents.end()) {
    return -1;
  }
  return std::distance(contents.begin(), iter);
}

// Adds the IPP |tag| to |buf|.
void AddTag(ipp_tag_t tag, SmartBuffer* buf) {
  CHECK_LE(tag, UCHAR_MAX) << "Tag value " << tag << " is too large";
  uint8_t tag_value = tag;
  buf->Add(&tag_value, sizeof(tag_value));
}

// Adds the IPP |name| to |bytes|. If |include_name| is set to true then the
// length of |name| and |name| itself are added to |buf|. Otherwise just a
// length with value 0 is added to indicate that there is no value.
void AddName(const std::string& name, bool include_name,
             SmartBuffer* buf) {
  CHECK_LE(name.size(), USHRT_MAX) << "Name size is too large";
  uint16_t name_length = (include_name) ? name.size() : 0;
  name_length = htons(name_length);
  buf->Add(&name_length, sizeof(name_length));
  if (include_name) {
    buf->Add(name.c_str(), name.size());
  }
}

// Adds the IPP |value_length| to |buf|.
void AddValueLength(size_t value_length, SmartBuffer* buf) {
  CHECK_LE(value_length, USHRT_MAX) << "Given value length is to large";
  uint16_t length = value_length;
  length = htons(length);
  buf->Add(&length, sizeof(length));
}

// Adds the IPP attribute of type bool to |buf|.
void AddBooleanAttribute(bool value, ipp_tag_t tag, const std::string& name,
                         bool include_name, SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(sizeof(value), buf);
  buf->Add(&value, sizeof(value));
}

// Adds the IPP attribute of type int to |buf|.
void AddIntAttribute(int value, ipp_tag_t tag, const std::string& name,
                     bool include_name, SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(sizeof(value), buf);
  // Need to put the int value in network byte order.
  value = htonl(value);
  buf->Add(&value, sizeof(value));
}

// Adds the IPP attribute of type string to |buf|.
void AddStringAttribute(const std::string& value, ipp_tag_t tag,
                        const std::string& name, bool include_name,
                        SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(value.size(), buf);
  buf->Add(value.c_str(), value.size());
}

}  // namespace

IppAttribute::IppAttribute(const std::string& type,
                           const std::string& name,
                           const base::Value* value)
    : type_(type), name_(name), value_(value) {}

bool IppAttribute::IsList() const {
  return value_->is_list();
}

size_t IppAttribute::GetListSize() const {
  const base::ListValue* values;
  CHECK(value_->GetAsList(&values))
      << "Failed to retrieve list value from " << name_;
  return values->GetSize();
}

bool IppAttribute::GetBool() const {
  bool out;
  CHECK(value_->GetAsBoolean(&out))
      << "Failed to retrieve boolean value from " << name_;
  return out;
}

int IppAttribute::GetInt() const {
  int out;
  CHECK(value_->GetAsInteger(&out))
      << "Failed to retrieve integer value from " << name_;
  return out;
}

std::string IppAttribute::GetString() const {
  std::string out;
  CHECK(value_->GetAsString(&out))
      << "Failed to retrieve string value from " << name_;
  return out;
}

std::vector<bool> IppAttribute::GetBools() const {
  const base::ListValue* values;
  CHECK(value_->GetAsList(&values))
      << "Failed to retrieve list value from " << name_;
  std::vector<bool> booleans(values->GetSize());
  for (std::size_t i = 0; i < values->GetSize(); ++i) {
    bool out;
    CHECK(values->GetBoolean(i, &out))
        << "Failed to retrieve boolean value from " << name_ << " at index "
        << i;
    booleans[i] = out;
  }
  return booleans;
}

std::vector<int> IppAttribute::GetInts() const {
  const base::ListValue* values;
  CHECK(value_->GetAsList(&values))
      << "Failed to retrieve list value from " << name_;
  std::vector<int> integers(values->GetSize());
  for (std::size_t i = 0; i < values->GetSize(); ++i) {
    int out;
    CHECK(values->GetInteger(i, &out))
        << "Failed to retrieve integer value from " << name_ << " at index "
        << i;
    integers[i] = out;
  }
  return integers;
}

std::vector<std::string> IppAttribute::GetStrings() const {
  const base::ListValue* values;
  CHECK(value_->GetAsList(&values))
      << "Failed to retrieve list value from " << name_;
  std::vector<std::string> strings(values->GetSize());
  for (std::size_t i = 0; i < values->GetSize(); ++i) {
    std::string out;
    CHECK(values->GetString(i, &out)) << "Failed to retrieve string value from "
                                      << name_ << " at index " << i;
    strings[i] = out;
  }
  return strings;
}

std::vector<uint8_t> IppAttribute::GetBytes() const {
  const base::ListValue* values;
  CHECK(value_->GetAsList(&values))
      << "Failed to retrieve list value from " << name_;
  std::vector<uint8_t> bytes(values->GetSize());
  for (std::size_t i = 0; i < values->GetSize(); ++i) {
    int out;
    CHECK(values->GetInteger(i, &out))
        << "Failed to retrieve byte value from " << name_ << " at index " << i;
    CHECK_GE(out, 0) << "Retrieved byte value is negative";
    CHECK_LE(out, UCHAR_MAX) << "Retrieved byte value is too large";
    bytes[i] = static_cast<uint8_t>(out);
  }
  return bytes;
}

bool operator==(const IppAttribute& lhs, const IppAttribute& rhs) {
  // TODO(valleau): Add comparison for |value_| members once chromelib is
  // updated and the operator is defined.
  return lhs.type_ == rhs.type_ && lhs.name_ == rhs.name_;
}

bool operator!=(const IppAttribute& lhs, const IppAttribute& rhs) {
  return !(lhs == rhs);
}

void UnpackIppHeader(IppHeader* header) {
  header->operation_id = ntohs(header->operation_id);
  header->request_id = ntohl(header->request_id);
}

void PackIppHeader(IppHeader* header) {
  header->operation_id = htons(header->operation_id);
  header->request_id = htonl(header->request_id);
}

IppHeader GetIppHeader(const SmartBuffer& message) {
  ssize_t start_of_header = FindFirstOccurrence(message, "\r\n\r\n");
  CHECK_NE(start_of_header, -1)
      << "The given message did not contain a valid HTTP header";
  start_of_header += 4;  // Skip past the "\r\n\r\n" end of header string.
  IppHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(&header, message.data() + start_of_header, sizeof(header));
  UnpackIppHeader(&header);
  return header;
}

std::string GetHttpResponseHeader(size_t size) {
  return "HTTP/1.1 200 OK\r\n"
         "Server: localhost:0\r\n"
         "Content-Type: application/ipp\r\n"
         "Content-Length: " + std::to_string(size) + "\r\n"
         "Connection: close\r\n\r\n";
}

IppAttribute GetAttribute(const base::Value* attribute) {
  const base::DictionaryValue* dictionary;
  CHECK(attribute->GetAsDictionary(&dictionary))
      << "Failed to retrieve dictionary value from attributes";

  std::string type;
  std::string name;
  const base::Value* value;
  CHECK(dictionary->GetString(kTypeKey, &type))
      << "Failed to retrieve type from attribute";
  CHECK(dictionary->GetString(kNameKey, &name))
      << "Failed to retrieve name from attribute";
  CHECK(dictionary->Get(kValueKey, &value))
      << "Failed to extract value from attribute";

  return IppAttribute(type, name, value);
}

std::vector<IppAttribute> GetAttributes(const base::Value* attributes,
                                        const std::string& key) {
  const base::DictionaryValue* dictionary;
  CHECK(attributes->GetAsDictionary(&dictionary))
      << "Failed to retrieve dictionary value from attributes";

  const base::ListValue* attributes_list;
  CHECK(dictionary->GetList(key, &attributes_list))
      << "Failed to extract attributes list for key " << key;

  return GetAttributes(attributes_list);
}

ipp_tag_t GetIppTag(const std::string& name) {
  std::map<std::string, ipp_tag_t> tags = {
      {kOperationAttributes, IPP_TAG_OPERATION},
      {kPrinterAttributes, IPP_TAG_PRINTER},
      {kNoValue, IPP_TAG_NOVALUE},
      {kInteger, IPP_TAG_INTEGER},
      {kBoolean, IPP_TAG_BOOLEAN},
      {kEnum, IPP_TAG_ENUM},
      {kOctetString, IPP_TAG_STRING},
      {kDateTime, IPP_TAG_DATE},
      {kResolution, IPP_TAG_RESOLUTION},
      {kRangeOfInteger, IPP_TAG_RANGE},
      {kBegCollection, IPP_TAG_BEGIN_COLLECTION},
      {kEndCollection, IPP_TAG_END_COLLECTION},
      {kTextWithoutLanguage, IPP_TAG_TEXT},
      {kNameWithoutLanguage, IPP_TAG_NAME},
      {kKeyword, IPP_TAG_KEYWORD},
      {kUri, IPP_TAG_URI},
      {kCharset, IPP_TAG_CHARSET},
      {kNaturalLanguage, IPP_TAG_LANGUAGE},
      {kMimeMediaType, IPP_TAG_MIMETYPE},
      {kMemberAttrName, IPP_TAG_MEMBERNAME}};
  auto iter = tags.find(name);
  if (iter == tags.end()) {
    LOG(ERROR) << "Given unknown tag name " << name;
    exit(1);
  }
  return iter->second;
}

void AddIppHeader(const IppHeader& header, SmartBuffer* buf) {
  IppHeader header_copy = header;
  PackIppHeader(&header_copy);
  buf->Add(&header_copy, sizeof(header_copy));
}

void AddEndOfAttributes(SmartBuffer* buf) {
  ipp_tag_t end_tag = IPP_TAG_END;
  CHECK_LE(end_tag, UCHAR_MAX);
  uint8_t tag_value = end_tag;
  buf->Add(&tag_value, sizeof(tag_value));
}

void AddPrinterAttributes(const std::vector<IppAttribute>& ipp_attributes,
                          const std::string& group, SmartBuffer* buf) {
  std::map<std::string,
           std::function<void(const IppAttribute&, SmartBuffer*)>>
      function_map = {{kNoValue, AddString},
                      {kInteger, AddInteger},
                      {kBoolean, AddBoolean},
                      {kEnum, AddInteger},
                      {kOctetString, AddOctetString},
                      {kDateTime, AddDate},
                      {kResolution, AddResolution},
                      {kRangeOfInteger, AddRange},
                      {kBegCollection, AddString},
                      {kEndCollection, AddString},
                      {kTextWithoutLanguage, AddString},
                      {kNameWithoutLanguage, AddString},
                      {kKeyword, AddString},
                      {kUri, AddString},
                      {kCharset, AddString},
                      {kNaturalLanguage, AddString},
                      {kMimeMediaType, AddString},
                      {kMemberAttrName, AddString}};

  // Add attribute group tag.
  ipp_tag_t group_tag = GetIppTag(group);
  CHECK_LE(group_tag, UCHAR_MAX) << "Tag for " << group << " is too large";
  uint8_t tag = group_tag;
  buf->Add(&tag, sizeof(tag));

  for (const IppAttribute& attribute : ipp_attributes) {
    auto iter = function_map.find(attribute.type());
    if (iter == function_map.end()) {
      LOG(ERROR) << "Found attribute with invalid type " << attribute.type();
      exit(1);
    }
    const auto& add_function = iter->second;
    add_function(attribute, buf);
  }
}

size_t GetBaseAttributeSize(const IppAttribute& attribute) {
  size_t multiplier = 1;
  // These types are special cases where although the values may be stored in a
  // list form, the tag and name fields only appear once.
  std::set<std::string> exempt = {kDateTime, kOctetString, kResolution,
                                  kRangeOfInteger};
  if (attribute.IsList() && exempt.find(attribute.type()) == exempt.end()) {
    multiplier = attribute.GetListSize();
  }
  // There are 3 elements which are repeated multiple times for each value in
  // |attribute|:
  //   tag - (1 byte)
  //   name-length (2 bytes)
  //   value-length (2 bytes)
  //
  // Which makes 5 bytes per value.
  return (5 * multiplier) + attribute.name().size();
}

size_t GetBooleanAttributeSize(const IppAttribute& attribute) {
  size_t total_size = GetBaseAttributeSize(attribute);
  if (attribute.IsList()) {
    total_size += attribute.GetListSize();
  } else {
    total_size += 1;
  }
  return total_size;
}

size_t GetIntAttributeSize(const IppAttribute& attribute) {
  size_t total_size = GetBaseAttributeSize(attribute);
  if (attribute.IsList()) {
    total_size += attribute.GetListSize() * 4;
  } else {
    total_size += 4;
  }
  return total_size;
}

size_t GetStringAttributeSize(const IppAttribute& attribute) {
  size_t total_size = GetBaseAttributeSize(attribute);
  if (attribute.IsList()) {
    const std::vector<std::string> values = attribute.GetStrings();
    for (const std::string& value : values) {
      total_size += value.size();
    }
  } else {
    const std::string value = attribute.GetString();
    total_size += value.size();
  }
  return total_size;
}

size_t GetOctetStringAttributeSize(const IppAttribute& attribute) {
  size_t total_size = GetBaseAttributeSize(attribute);
  if (attribute.IsList()) {
    total_size += attribute.GetListSize();
  } else {
    const std::string value = attribute.GetString();
    total_size += value.size();
  }
  return total_size;
}

size_t GetDateTimeAttributeSize(const IppAttribute& attribute) {
  return GetBaseAttributeSize(attribute) + kDateTimeSize;
}

size_t GetResolutionAttributeSize(const IppAttribute& attribute) {
  return GetBaseAttributeSize(attribute) + kResolutionSize;
}

size_t GetRangeOfIntegerAttributeSize(const IppAttribute& attribute) {
  return GetBaseAttributeSize(attribute) + kResolutionSize;
}

size_t GetAttributesSize(const std::vector<IppAttribute>& attributes) {
  std::map<std::string, std::function<size_t(const IppAttribute&)>>
      function_map = {{kNoValue, GetStringAttributeSize},
                      {kInteger, GetIntAttributeSize},
                      {kBoolean, GetBooleanAttributeSize},
                      {kEnum, GetIntAttributeSize},
                      {kOctetString, GetOctetStringAttributeSize},
                      {kDateTime, GetDateTimeAttributeSize},
                      {kResolution, GetResolutionAttributeSize},
                      {kRangeOfInteger, GetRangeOfIntegerAttributeSize},
                      {kBegCollection, GetStringAttributeSize},
                      {kEndCollection, GetStringAttributeSize},
                      {kTextWithoutLanguage, GetStringAttributeSize},
                      {kNameWithoutLanguage, GetStringAttributeSize},
                      {kKeyword, GetStringAttributeSize},
                      {kUri, GetStringAttributeSize},
                      {kCharset, GetStringAttributeSize},
                      {kNaturalLanguage, GetStringAttributeSize},
                      {kMimeMediaType, GetStringAttributeSize},
                      {kMemberAttrName, GetStringAttributeSize}};
  size_t total_size = 0;
  for (const IppAttribute& attribute : attributes) {
    auto iter = function_map.find(attribute.type());
    if (iter == function_map.end()) {
      LOG(ERROR) << "Found attribute with invalid type " << attribute.type();
      exit(1);
    }
    const auto& size_function = iter->second;
    total_size += size_function(attribute);
  }
  return total_size;
}

void AddBoolean(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  if (attribute.IsList()) {
    std::vector<bool> values = attribute.GetBools();
    for (size_t i = 0; i < values.size(); ++i) {
      // We only include the name of the attribute for the first value.
      bool include_name = (i == 0);
      AddBooleanAttribute(values[i], tag, name, include_name, buf);
    }
  } else {
    bool value = attribute.GetBool();
    AddBooleanAttribute(value, tag, name, true, buf);
  }
}

void AddInteger(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  if (attribute.IsList()) {
    std::vector<int> values = attribute.GetInts();
    for (size_t i = 0; i < values.size(); ++i) {
      // We only include the name of the attribute for the first value.
      bool include_name = (i == 0);
      AddIntAttribute(values[i], tag, name, include_name, buf);
    }
  } else {
    int value = attribute.GetInt();
    AddIntAttribute(value, tag, name, true, buf);
  }
}

void AddString(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  if (attribute.IsList()) {
    std::vector<std::string> values = attribute.GetStrings();
    for (size_t i = 0; i < values.size(); ++i) {
      // We only include the name of the attribute for the first value.
      bool include_name = (i == 0);
      AddStringAttribute(values[i], tag, name, include_name, buf);
    }
  } else {
    std::string value = attribute.GetString();
    AddStringAttribute(value, tag, name, true, buf);
  }
}

void AddOctetString(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  if (attribute.IsList()) {
    std::vector<uint8_t> values = attribute.GetBytes();
    AddTag(tag, buf);
    AddName(name, true, buf);
    AddValueLength(values.size(), buf);
    buf->Add(values.data(), values.size());
  } else {
    std::string value = attribute.GetString();
    AddStringAttribute(value, tag, name, true, buf);
  }
}

void AddDate(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  CHECK(attribute.IsList()) << "Date value is in an incorrect format";

  AddTag(tag, buf);
  AddName(name, true, buf);
  AddValueLength(kDateTimeSize, buf);

  std::vector<uint8_t> date = attribute.GetBytes();
  buf->Add(date.data(), date.size());
}

void AddRange(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();

  CHECK(attribute.IsList()) << "Range value is in an incorrect format";
  std::vector<int> range = attribute.GetInts();
  CHECK_EQ(range.size(), 2) << "Range list is an invalid size";

  AddTag(tag, buf);
  AddName(name, true, buf);
  AddValueLength(kRangeOfIntegerSize, buf);

  for (int value : range) {
    // Need to put the int value in network byte order.
    value = htonl(value);
    buf->Add(&value, sizeof(value));
  }
}

void AddResolution(const IppAttribute& attribute, SmartBuffer* buf) {
  ipp_tag_t tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();

  CHECK(attribute.IsList()) << "Resolution value is in an incorrect format";
  std::vector<int> resolution = attribute.GetInts();
  CHECK_EQ(resolution.size(), 3) << "Resolution list is an invalid size";
  CHECK_LE(resolution[2], UCHAR_MAX) << "Resolution units value is too large";

  AddTag(tag, buf);
  AddName(name, true, buf);
  AddValueLength(kResolutionSize, buf);

  for (size_t i = 0; i < resolution.size() - 1; ++i) {
    int value = resolution[i];
    // Need to put the int value in network byte order.
    value = htonl(value);
    buf->Add(&value, sizeof(value));
  }

  uint8_t units = resolution[2];
  buf->Add(&units, sizeof(units));
}
