// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escl_manager.h"

#include <utility>

#include <base/logging.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "xml_util.h"

using base::Value;
using ::testing::ElementsAre;

namespace {

Value CreateCapabilitiesJson() {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("MakeAndModel", Value("Test Make and Model"));
  dict.SetKey("SerialNumber", Value("Test Serial"));

  Value platen(Value::Type::DICTIONARY);
  Value::ListStorage color_modes;
  color_modes.push_back(Value("RGB24"));
  color_modes.push_back(Value("Grayscale8"));
  platen.SetKey("ColorModes", Value(std::move(color_modes)));

  Value::ListStorage document_formats;
  document_formats.push_back(Value("application/pdf"));
  platen.SetKey("DocumentFormats", Value(std::move(document_formats)));

  Value::ListStorage resolutions;
  resolutions.push_back(Value(100));
  resolutions.push_back(Value(200));
  resolutions.push_back(Value(300));
  platen.SetKey("Resolutions", Value(std::move(resolutions)));

  dict.SetKey("Platen", std::move(platen));

  return dict;
}

inline const xmlChar* XmlCast(const char* p) {
  return reinterpret_cast<const xmlChar*>(p);
}

}  // namespace

TEST(ScannerCapabilities, Initialize) {
  base::Optional<ScannerCapabilities> opt_caps =
      CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(opt_caps);

  ScannerCapabilities caps = opt_caps.value();
  EXPECT_EQ(caps.make_and_model, "Test Make and Model");
  EXPECT_EQ(caps.serial_number, "Test Serial");
  EXPECT_THAT(caps.platen_capabilities.color_modes,
              ElementsAre("RGB24", "Grayscale8"));
  EXPECT_THAT(caps.platen_capabilities.formats, ElementsAre("application/pdf"));
  EXPECT_THAT(caps.platen_capabilities.resolutions, ElementsAre(100, 200, 300));
}

TEST(ScannerCapabilities, InitializeFailColorModeHasInteger) {
  Value json = CreateCapabilitiesJson();
  json.FindPath({"Platen", "ColorModes"})->GetList().push_back(Value(9));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailDocumentFormatsHasDouble) {
  Value json = CreateCapabilitiesJson();
  json.FindPath({"Platen", "DocumentFormats"})->GetList().push_back(Value(2.7));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailResolutionsHasString) {
  Value json = CreateCapabilitiesJson();
  json.FindPath({"Platen", "Resolutions"})->GetList().push_back(Value("600"));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingMakeAndModel) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveKey("MakeAndModel"));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingSerialNumber) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveKey("SerialNumber"));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingColorModes) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemovePath({"Platen", "ColorModes"}));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingDocumentFormats) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemovePath({"Platen", "DocumentFormats"}));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingResolutions) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemovePath({"Platen", "Resolutions"}));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingPlatenSection) {
  Value json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveKey("Platen"));
  EXPECT_FALSE(CreateScannerCapabilitiesFromConfig(std::move(json)));
}

// Despite taking a mutable pointer to |doc|, this does not modify |doc|.
void HasPathWithContents(xmlDoc* doc,
                         const std::string& path,
                         std::vector<std::string> expected_contents) {
  xmlXPathContext* context = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(context, XmlCast("scan"),
                     XmlCast("http://schemas.hp.com/imaging/escl/2011/05/03"));
  xmlXPathRegisterNs(context, XmlCast("pwg"),
                     XmlCast("http://www.pwg.org/schemas/2010/12/sm"));
  xmlXPathObject* result =
      xmlXPathEvalExpression(XmlCast(path.c_str()), context);
  ASSERT_FALSE(xmlXPathNodeSetIsEmpty(result->nodesetval))
      << "No nodes found with path " << path;
  int node_count = result->nodesetval->nodeNr;
  ASSERT_EQ(node_count, expected_contents.size())
      << "Found " << node_count << " nodes, but " << expected_contents.size()
      << " were expected.";
  for (int i = 0; i < node_count; i++) {
    const xmlNode* node = result->nodesetval->nodeTab[i];
    xmlChar* node_text = xmlNodeGetContent(node);
    EXPECT_EQ(reinterpret_cast<char*>(node_text), expected_contents[i]);
    xmlFree(node_text);
  }
  xmlXPathFreeContext(context);
  xmlXPathFreeObject(result);
}

TEST(ScannerCapabilities, AsXml) {
  base::Optional<ScannerCapabilities> caps =
      CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  std::vector<uint8_t> xml = ScannerCapabilitiesAsXml(caps.value());
  xmlDoc* doc = xmlReadMemory(reinterpret_cast<const char*>(xml.data()),
                              xml.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);

  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:Version", {"2.63"});
  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:MakeAndModel",
                      {"Test Make and Model"});
  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:SerialNumber",
                      {"Test Serial"});

  const std::string platen_caps =
      "/scan:ScannerCapabilities/scan:Platen/scan:PlatenInputCaps";
  const std::string profiles = "/scan:SettingProfiles/scan:SettingProfile";
  HasPathWithContents(
      doc, platen_caps + profiles + "/scan:ColorModes/scan:ColorMode",
      {"RGB24", "Grayscale8"});
  HasPathWithContents(
      doc, platen_caps + profiles + "/scan:DocumentFormats/pwg:DocumentFormat",
      {"application/pdf"});
  HasPathWithContents(doc,
                      platen_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:XResolution",
                      {"100", "200", "300"});
  xmlFreeDoc(doc);
}
