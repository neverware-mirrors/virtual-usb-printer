// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CUPS_CONSTANTS_H__
#define CUPS_CONSTANTS_H__

#include <iostream>
#include <type_traits>

// The values for these tags are defined in IPP RFC 8010:
// https://tools.ietf.org/html/rfc8010#section-3.5
//
// Be careful about adding new entries to this enum because they can change the
// underlying values of existing entries and cause failures.
enum class IppTag : uint8_t {
  ZERO = 0x00,
  OPERATION,
  JOB,
  END,
  PRINTER,
  UNSUPPORTED_GROUP,
  SUBSCRIPTION,
  EVENT_NOTIFICATION,
  RESOURCE,
  DOCUMENT,
  UNSUPPORTED_VALUE = 0x10,
  DEFAULT,
  UNKNOWN,
  NOVALUE,
  NOTSETTABLE = 0x15,
  DELETEATTR,
  ADMINDEFINE,
  INTEGER = 0x21,
  BOOLEAN,
  ENUM,
  STRING = 0x30,
  DATE,
  RESOLUTION,
  RANGE,
  BEGIN_COLLECTION,
  TEXTLANG,
  NAMELANG,
  END_COLLECTION,
  TEXT = 0x41,
  NAME,
  RESERVED_STRING,
  KEYWORD,
  URI,
  URISCHEME,
  CHARSET,
  LANGUAGE,
  MIMETYPE,
  MEMBERNAME
};

std::ostream& operator <<(std::ostream& os, const IppTag& tag);

#endif  // CUPS_CONSTANTS_H__
