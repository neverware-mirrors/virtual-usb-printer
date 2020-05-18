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

// An eSCL ScanSettings structure serialized to XML. This is used to test our
// XML parsing code, as well as to test handling of requests to the ScanJob
// endpoint that wish to create new scans.
// clang-format off
constexpr char kNewScan[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>png</pwg:DocumentFormat>"
       "<scan:ColorMode>BlackAndWhite1</scan:ColorMode>"
       "<scan:XResolution>300</scan:XResolution>"
       "<scan:YResolution>300</scan:YResolution>"
       "<pwg:InputSource>Platen</pwg:InputSource>"
       "<scan:InputSource>Platen</scan:InputSource>"
   "</scan:ScanSettings>";
// clang-format on

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

// Test that we can properly parse a ScanSettings document.
TEST(ScanSettings, Parse) {
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  base::Optional<ScanSettings> opt_settings = ScanSettingsFromXml(xml);
  EXPECT_TRUE(opt_settings);
  ScanSettings settings = opt_settings.value();
  EXPECT_EQ(settings.document_format, "png");
  EXPECT_EQ(settings.color_mode, kBlackAndWhite);
  EXPECT_EQ(settings.input_source, "Platen");
  EXPECT_EQ(settings.x_resolution, 300);
  EXPECT_EQ(settings.y_resolution, 300);

  EXPECT_EQ(settings.regions.size(), 1);
  ScanRegion region = settings.regions.at(0);
  EXPECT_EQ(region.units, "escl:ThreeHundredthsOfInches");
  EXPECT_EQ(region.height, 600);
  EXPECT_EQ(region.width, 200);
  EXPECT_EQ(region.x_offset, 0);
  EXPECT_EQ(region.y_offset, 0);
}

TEST(HandleEsclRequest, InvalidEndpoint) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/InvalidEndpoint";

  ScannerCapabilities caps;
  EsclManager manager(caps);
  HttpResponse response = manager.HandleEsclRequest(request);
  EXPECT_EQ(response.status, "404 Not Found");
  EXPECT_EQ(response.body.size(), 0);
}

// Tests that a GET request to /eSCL/ScannerCapabilities returns a valid XML
// response.
// TODO(b/157735732): add validation logic once we can.
TEST(HandleEsclRequest, ScannerCapabilities) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/ScannerCapabilities";

  base::Optional<ScannerCapabilities> caps =
      CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value());
  HttpResponse response = manager.HandleEsclRequest(request);
  EXPECT_EQ(response.status, "200 OK");
  EXPECT_GT(response.body.size(), 0);

  xmlDoc* doc =
      xmlReadMemory(reinterpret_cast<const char*>(response.body.data()),
                    response.body.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);
  xmlFreeDoc(doc);
}

// Tests that a GET request to /eSCL/ScannerStatus returns a valid XML
// response.
// TODO(b/157735732): add validation logic once we can.
TEST(HandleEsclRequest, ScannerStatus) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/ScannerStatus";

  ScannerCapabilities caps;
  EsclManager manager(caps);
  HttpResponse response = manager.HandleEsclRequest(request);
  EXPECT_EQ(response.status, "200 OK");
  EXPECT_GT(response.body.size(), 0);

  xmlDoc* doc =
      xmlReadMemory(reinterpret_cast<const char*>(response.body.data()),
                    response.body.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);
  xmlFreeDoc(doc);
}
