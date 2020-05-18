// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __XML_UTIL_H__
#define __XML_UTIL_H__

#include <stdint.h>

#include <vector>

#include <base/optional.h>

#include "escl_manager.h"

// Returns a serialized eSCL ScannerCapabilities XML representation of |caps|.
// For fields that are not provided by |caps|, sensible default values are
// chosen.
std::vector<uint8_t> ScannerCapabilitiesAsXml(const ScannerCapabilities& caps);

// Returns a serialized eSCL ScannerStatus XML representation of |status|.
std::vector<uint8_t> ScannerStatusAsXml(const ScannerStatus& status);

// Attempts to parse a ScanSettings object from its xml representation, |xml|.
base::Optional<ScanSettings> ScanSettingsFromXml(
    const std::vector<uint8_t>& xml);

#endif  // __XML_UTIL_H__
