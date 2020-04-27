// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipp_util.h"

#include <map>
#include <set>
#include <string>

#include <arpa/inet.h>
#include <base/optional.h>
#include <base/values.h>

#include "smart_buffer.h"
#include "usbip_constants.h"

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

// Determines if |message| starts with the string |target|.
bool StartsWith(const SmartBuffer& message, const std::string& target) {
  return message.FindFirstOccurrence(target) == 0;
}

bool MessageContains(const SmartBuffer& message,
                     const std::string& s) {
  ssize_t pos = message.FindFirstOccurrence(s);
  return pos != -1;
}

// Adds the IPP |tag| to |buf|.
void AddTag(IppTag tag, SmartBuffer* buf) {
  uint8_t tag_value = static_cast<uint8_t>(tag);
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
  CHECK_LE(value_length, USHRT_MAX) << "Given value length is too large";
  uint16_t length = value_length;
  length = htons(length);
  buf->Add(&length, sizeof(length));
}

// Adds the IPP attribute of type bool to |buf|.
void AddBooleanAttribute(bool value, IppTag tag, const std::string& name,
                         bool include_name, SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(sizeof(value), buf);
  buf->Add(&value, sizeof(value));
}

// Adds the IPP attribute of type int to |buf|.
void AddIntAttribute(int value, IppTag tag, const std::string& name,
                     bool include_name, SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(sizeof(value), buf);
  // Need to put the int value in network byte order.
  value = htonl(value);
  buf->Add(&value, sizeof(value));
}

// Adds the IPP attribute of type string to |buf|.
void AddStringAttribute(const std::string& value, IppTag tag,
                        const std::string& name, bool include_name,
                        SmartBuffer* buf) {
  AddTag(tag, buf);
  AddName(name, include_name, buf);
  AddValueLength(value.size(), buf);
  buf->Add(value.c_str(), value.size());
}

base::Optional<uint16_t> ReadShort(const std::vector<uint8_t>& bytes,
                                   size_t start) {
  uint16_t value;
  if (start + sizeof(value) > bytes.size()) {
    LOG(ERROR) << "Could not read short value, there are not 2 bytes available";
    return base::nullopt;
  }

  memcpy(&value, &bytes[start], sizeof(value));
  value = ntohs(value);
  return value;
}

// Get the length of an IPP attribute in |bytes| starting at index |start|.
// If |bytes| does not contain a well-formed attribute, return nullopt.
base::Optional<size_t> GetAttributeLength(const std::vector<uint8_t>& bytes,
                                          size_t start) {
  size_t i = start;
  base::Optional<uint16_t> name_length = ReadShort(bytes, i);
  if (!name_length) {
    LOG(ERROR) << "Failed reading name length at index " << i;
    return base::nullopt;
  }
  i += 2;
  i += name_length.value();
  base::Optional<uint16_t> value_length = ReadShort(bytes, i);
  if (!value_length) {
    LOG(ERROR) << "Failed reading value length at index " << i;
    return base::nullopt;
  }
  i += 2;
  i += value_length.value();
  if (i > bytes.size()) {
    LOG(ERROR) << "Value length " << value_length.value()
               << " exceeds size of buffer";
    return base::nullopt;
  }
  return i - start;
}

// Valid tag range defined in RFC 8010:
// https://tools.ietf.org/html/rfc8010#section-3.2
// See 'begin-attribute-group-tag'
bool IsAttributeGroupTag(uint8_t tag) {
  return (0x00 <= tag && tag < 0x02) || (0x04 <= tag && tag <= 0x0F);
}

// Get the length of an IPP attribute group in |bytes| starting at index
// |start|. Do this by summing the lengths of each attribute within the group.
// If |bytes| does not contain a well-formed attribute group, return nullopt.
base::Optional<size_t> GetGroupLength(const std::vector<uint8_t>& bytes,
                                      size_t start) {
  if (start >= bytes.size())
    return base::nullopt;

  size_t i = start;
  while (i < bytes.size()) {
    uint8_t tag = bytes[i];
    if (tag == static_cast<uint8_t>(IppTag::END) || IsAttributeGroupTag(tag)) {
      // Reached end of group.
      return i - start;
    }
    // Skip the tag.
    i++;

    // Skip the attribute.
    base::Optional<size_t> length = GetAttributeLength(bytes, i);
    if (!length) {
      LOG(ERROR) << "Failed to parse attribute at index " << i;
      return base::nullopt;
    }
    i += length.value();
  }
  LOG(ERROR) << "Reached end of group without finding END or new group tag";
  return base::nullopt;
}

// Get the length of the IPP attributes section at the beginning of |bytes|.
// Do this by summing the lengths of each attribute group.
// If |bytes| does not contain a well-formed attributes section, return nullopt.
base::Optional<size_t> GetAttributesLength(const std::vector<uint8_t>& bytes) {
  size_t i = 0;
  while (i < bytes.size()) {
    uint8_t tag = bytes[i++];
    if (tag == static_cast<uint8_t>(IppTag::END)) {
      return i;
    } else if (IsAttributeGroupTag(tag)) {
      // This is the start of a group
      base::Optional<size_t> length = GetGroupLength(bytes, i);
      if (!length) {
        LOG(ERROR) << "Failed to parse group at index " << i;
        return base::nullopt;
      }
      i += length.value();
    } else {
      LOG(ERROR) << "Invalid attribute group tag '" << static_cast<int>(tag)
                 << "'";
      return base::nullopt;
    }
  }
  // Attributes must end with an END tag, so if we reach here it is invalid.
  LOG(ERROR) << "Reached end of buffer without finding END tag";
  return base::nullopt;
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

bool IsHttpChunkedMessage(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("Transfer-Encoding: chunked");
  return i != -1;
}

bool ContainsHttpHeader(const SmartBuffer& message) {
  return MessageContains(message, "POST /ipp/print HTTP");
}

bool ContainsHttpBody(const SmartBuffer& message) {
  // We are making the assumption that if |message| does not contain an HTTP
  // header then |message| is the body of an HTTP message.
  if (!ContainsHttpHeader(message)) {
    return true;
  }
  // If |message| contains an HTTP header, check to see if there's anything
  // immediately following it.
  ssize_t pos = message.FindFirstOccurrence("\r\n\r\n");
  CHECK_GE(pos, 0) << "HTTP header does not contain end-of-header marker";
  size_t start = pos + 4;
  return start < message.size();
}

IppHeader GetIppHeader(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("\r\n\r\n");
  size_t offset = 0;
  if (i != -1) {
    // If |message| starts with an HTTP header, jump to the beginning of the IPP
    // message.
    if (IsHttpChunkedMessage(message)) {
      // If |message| is an HTTP chunked message header, jump to the beginning
      // of the first chunk.
      i = message.FindFirstOccurrence("\r\n", i + 4);
      offset = i + 2;
    } else {
      offset = i + 4;
    }
  }
  IppHeader header;
  // Ensure that |message| has enough bytes to copy into |header|.
  CHECK_GE(message.size(), offset);
  CHECK_GE(message.size() - offset, sizeof(header));
  memset(&header, 0, sizeof(header));
  memcpy(&header, message.data() + offset, sizeof(header));
  UnpackIppHeader(&header);
  return header;
}

bool RemoveIppAttributes(SmartBuffer* buf) {
  base::Optional<size_t> length = GetAttributesLength(buf->contents());
  if (!length) {
    LOG(ERROR) << "Buffer does not contain well-formed IPP attributes";
    return false;
  }

  buf->Erase(0, length.value());
  return true;
}

size_t ExtractChunkSize(const SmartBuffer& message) {
  ssize_t end = message.FindFirstOccurrence("\r\n");
  if (end == -1) {
    return 0;
  }
  std::string hex_string;
  const std::vector<uint8_t>& contents = message.contents();
  for (size_t i = 0; i < end; ++i) {
    hex_string += static_cast<char>(contents[i]);
  }
  return std::stoll(hex_string, 0, 16);
}

SmartBuffer ParseHttpChunkedMessage(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Given null message";
  // If |message| starts with the trailing CRLF end-of-chunk indicator from the
  // previous chunk then erase it.
  if (StartsWith(*message, "\r\n")) {
    message->Erase(0, 2);
  }
  size_t chunk_size = ExtractChunkSize(*message);
  LOG(INFO) << "Chunk size: " << chunk_size;
  ssize_t start = message->FindFirstOccurrence("\r\n");
  SmartBuffer ret(0);
  if (start == -1) {
    return ret;
  }

  ret.Add(*message, start + 2, chunk_size);
  // In case |message| contains multiple chunks, we remove the chunk which was
  // just parsed.
  //
  // The length of the prefix to be deleted is calculated as follows:
  // start      - represents the hex-encoded length value.
  // chunk_size - the number of bytes read out for the message body.
  // 2          - the CRLF characters which trail the length.
  size_t to_erase_length = start + chunk_size + 2;
  CHECK_GE(message->size(), to_erase_length);
  message->Erase(0, to_erase_length);
  // If |message| also contains the trailing CRLF end-of-chunk indicator, then
  // erase it.
  if (StartsWith(*message, "\r\n")) {
    message->Erase(0, 2);
  }

  return ret;
}

bool ContainsFinalChunk(const SmartBuffer& message) {
  ssize_t i = message.FindFirstOccurrence("0\r\n\r\n");
  return i > 0 && message.size() == i + 5;
}

bool ProcessMessageChunks(SmartBuffer* message) {
  CHECK(message != nullptr) << "Process - Given null message";
  if (IsHttpChunkedMessage(*message)) {
    // If |message| contains an HTTP header then we discard it.
    int start = message->FindFirstOccurrence("\r\n\r\n");
    CHECK_GT(start, 0) << "Failed to process HTTP chunked message";
    message->Erase(0, start + 4);
  }

  SmartBuffer chunk;
  while (message->size() > 0) {
    chunk = ParseHttpChunkedMessage(message);
  }
  return chunk.size() != 0;
}

void RemoveHttpHeader(SmartBuffer* message) {
  CHECK(ContainsHttpHeader(*message)) << "Message does not contain HTTP header";
  int i = message->FindFirstOccurrence("\r\n\r\n");
  CHECK_GT(i, 0) << "Failed to find end of HTTP header";
  message->Erase(0, i + 4);
}

SmartBuffer ExtractIppMessage(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Received null message";
  return ParseHttpChunkedMessage(message);
}

SmartBuffer MergeDocument(SmartBuffer* message) {
  CHECK_NE(message, nullptr) << "Received null message";
  SmartBuffer document;
  while (message->size() > 0) {
    SmartBuffer chunk = ParseHttpChunkedMessage(message);
    document.Add(chunk);
  }
  return document;
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

IppTag GetIppTag(const std::string& name) {
  std::map<std::string, IppTag> tags = {
      {kOperationAttributes, IppTag::OPERATION},
      {kUnsupportedAttributes, IppTag::UNSUPPORTED_GROUP},
      {kPrinterAttributes, IppTag::PRINTER},
      {kJobAttributes, IppTag::JOB},
      {kUnsupported, IppTag::UNSUPPORTED_VALUE},
      {kNoValue, IppTag::NOVALUE},
      {kInteger, IppTag::INTEGER},
      {kBoolean, IppTag::BOOLEAN},
      {kEnum, IppTag::ENUM},
      {kOctetString, IppTag::STRING},
      {kDateTime, IppTag::DATE},
      {kResolution, IppTag::RESOLUTION},
      {kRangeOfInteger, IppTag::RANGE},
      {kBegCollection, IppTag::BEGIN_COLLECTION},
      {kEndCollection, IppTag::END_COLLECTION},
      {kTextWithoutLanguage, IppTag::TEXT},
      {kNameWithoutLanguage, IppTag::NAME},
      {kKeyword, IppTag::KEYWORD},
      {kUri, IppTag::URI},
      {kCharset, IppTag::CHARSET},
      {kNaturalLanguage, IppTag::LANGUAGE},
      {kMimeMediaType, IppTag::MIMETYPE},
      {kMemberAttrName, IppTag::MEMBERNAME}};
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
  IppTag end_tag = IppTag::END;
  uint8_t tag_value = static_cast<uint8_t>(end_tag);
  buf->Add(&tag_value, sizeof(tag_value));
}

void AddPrinterAttributes(const std::vector<IppAttribute>& ipp_attributes,
                          const std::string& group, SmartBuffer* buf) {
  std::map<std::string,
           std::function<void(const IppAttribute&, SmartBuffer*)>>
      function_map = {{kUnsupported, AddString},
                      {kNoValue, AddString},
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
  IppTag group_tag = GetIppTag(group);
  uint8_t tag = static_cast<uint8_t>(group_tag);
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
      function_map = {{kUnsupported, GetStringAttributeSize},
                      {kNoValue, GetStringAttributeSize},
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
  IppTag tag = GetIppTag(attribute.type());
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
  IppTag tag = GetIppTag(attribute.type());
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
  IppTag tag = GetIppTag(attribute.type());
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
  IppTag tag = GetIppTag(attribute.type());
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
  IppTag tag = GetIppTag(attribute.type());
  const std::string name = attribute.name();
  CHECK(attribute.IsList()) << "Date value is in an incorrect format";

  AddTag(tag, buf);
  AddName(name, true, buf);
  AddValueLength(kDateTimeSize, buf);

  std::vector<uint8_t> date = attribute.GetBytes();
  buf->Add(date.data(), date.size());
}

void AddRange(const IppAttribute& attribute, SmartBuffer* buf) {
  IppTag tag = GetIppTag(attribute.type());
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
  IppTag tag = GetIppTag(attribute.type());
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
