// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __IPP_UTIL_H__
#define __IPP_UTIL_H__

#include <cstring>
#include <string>
#include <vector>

#include <base/values.h>

#include "cups_constants.h"
#include "smart_buffer.h"
#include "usbip_constants.h"

// The JSON keys we expect to see in an IPP attribute object.
constexpr char kTypeKey[] = "type";
constexpr char kNameKey[] = "name";
constexpr char kValueKey[] = "value";

// The names of the attribute groups.
constexpr char kOperationAttributes[] = "operationAttributes";
constexpr char kUnsupportedAttributes[] = "unsupportedAttributes";
constexpr char kPrinterAttributes[] = "printerAttributes";
constexpr char kJobAttributes[] = "jobAttributes";

// The type names that can be seen in an ipp attribute.
constexpr char kUnsupported[] = "unsupported";
constexpr char kNoValue[] = "no-value";
constexpr char kInteger[] = "integer";
constexpr char kBoolean[] = "boolean";
constexpr char kEnum[] = "enum";
constexpr char kOctetString[] = "octetString";
constexpr char kDateTime[] = "dateTime";
constexpr char kResolution[] = "resolution";
constexpr char kRangeOfInteger[] = "rangeOfInteger";
constexpr char kBegCollection[] = "begCollection";
constexpr char kEndCollection[] = "endCollection";
constexpr char kTextWithoutLanguage[] = "textWithoutLanguage";
constexpr char kNameWithoutLanguage[] = "nameWithoutLanguage";
constexpr char kKeyword[] = "keyword";
constexpr char kUri[] = "uri";
constexpr char kCharset[] = "charset";
constexpr char kNaturalLanguage[] = "naturalLanguage";
constexpr char kMimeMediaType[] = "mimeMediaType";
constexpr char kMemberAttrName[] = "memberAttrName";

// Every IPP attribute of type dateTime should be 11 bytes.
const size_t kDateTimeSize = 11;
// Every IPP attribute of type rangeOfInteger should be 8 bytes.
const size_t kRangeOfIntegerSize = 8;
// Every IPP attribute of type resolution should be 9 bytes.
const size_t kResolutionSize = 9;

// Represents a single IPP attribute loaded from the JSON configuration file.
// Serves as an accessor for the underlying base::Value object. This class does
// not own the base::Value object and must not outlive it.
class IppAttribute {
 public:
  explicit IppAttribute(const std::string& type,
                        const std::string& name,
                        const base::Value* value);

  std::string type() const { return type_; }
  std::string name() const { return name_; }
  const base::Value* value() const { return value_; }

  const char* name_cstr() const { return name_.c_str(); }

  bool IsList() const;
  size_t GetListSize() const;

  bool GetBool() const;
  int GetInt() const;
  std::string GetString() const;

  std::vector<bool> GetBools() const;
  std::vector<int> GetInts() const;
  std::vector<std::string> GetStrings() const;
  std::vector<uint8_t> GetBytes() const;

  bool friend operator==(const IppAttribute& lhs, const IppAttribute& rhs);
  bool friend operator!=(const IppAttribute& lhs, const IppAttribute& rhs);

 private:
  std::string type_;
  std::string name_;
  const base::Value* value_;
};

class IppHeader {
 public:
  // Attempts to parse an IppHeader from the beginning of |message|.
  // If successful, removes the header from |message|, so that it contains only
  // the body of the ipp request.
  // If unsuccessful, does not modify |message|.
  static base::Optional<IppHeader> Deserialize(SmartBuffer* message);

  // Append this IppHeader to |buf|.
  void Serialize(SmartBuffer* buf);

  // Do not re-order or add to these fields. They are laid out such that they
  // match the serialized format of an IppHeader (apart from byte ordering).
  uint8_t major;
  uint8_t minor;
  // NOTE: operation_id is treated as a status value in an IPP response.
  uint16_t operation_id;
  int request_id;
};

void PackIppHeader(IppHeader* header);
void UnpackIppHeader(IppHeader* header);

IppHeader GetIppHeader(const SmartBuffer& message);

// Strip leading IPP attributes from |buf|.
// Returns true and strips leading attributes if |buf| starts with well-formed
// IPP attributes.
// Returns false and does not modify |buf| if |buf| does not contain any
// attributes.
bool RemoveIppAttributes(SmartBuffer* buf);

// Construct an IppAttribute object for the given |attribute| which should be a
// JSON representation of a single IPP attribute.
IppAttribute GetAttribute(const base::Value* attribute);

// Extracts ipp attributes from the |attributes| JSON.
std::vector<IppAttribute> GetAttributes(const base::Value* attributes,
                                        const std::string& key);

// Converts the |name| of a tag into its corresponding value from cups.
IppTag GetIppTag(const std::string& group);

void AddEndOfAttributes(SmartBuffer* buf);

void AddPrinterAttributes(const std::vector<IppAttribute>& ipp_attributes,
                          const std::string& group, SmartBuffer* buf);

// Determine the number of bytes required to write the portion of |attribute|
// which is the same regardless of the underlying value type to a buffer.
size_t GetBaseAttributeSize(const IppAttribute& attribute);

// Determines the number of bytes required to write the given IppAttribute
// |attribute| to a buffer based on the underlying value type.
size_t GetBooleanAttributeSize(const IppAttribute& attribute);
size_t GetIntAttributeSize(const IppAttribute& attribute);
size_t GetStringAttributeSize(const IppAttribute& attribute);
size_t GetOctetStringAttributeSize(const IppAttribute& attribute);
size_t GetAttributesSize(const std::vector<IppAttribute>& attribute);

void AddBoolean(const IppAttribute& attribute, SmartBuffer* buf);
void AddInteger(const IppAttribute& attribute, SmartBuffer* buf);
void AddString(const IppAttribute& attribute, SmartBuffer* buf);
void AddOctetString(const IppAttribute& attribute, SmartBuffer* buf);
void AddDate(const IppAttribute& attribute, SmartBuffer* buf);
void AddRange(const IppAttribute& attribute, SmartBuffer* buf);
void AddResolution(const IppAttribute& attribute, SmartBuffer* buf);

#endif  // __IPP_UTIL_H__
