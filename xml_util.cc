// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xml_util.h"

#include <memory>
#include <utility>
#include <string>

#include <base/containers/flat_map.h>
#include <libxml/tree.h>

namespace {

struct XmlNodeDeleter {
  void operator()(xmlNode* p) { xmlFreeNode(p); }
};
using UniqueXmlNodePtr = std::unique_ptr<xmlNode, XmlNodeDeleter>;

struct XmlDocDeleter {
  void operator()(xmlDoc* p) { xmlFreeDoc(p); }
};
using UniqueXmlDocPtr = std::unique_ptr<xmlDoc, XmlDocDeleter>;

constexpr char kPwgNs[] = "pwg";
constexpr char kScanNs[] = "scan";
using namespace_map = base::flat_map<std::string, xmlNs*>;

inline const xmlChar* XmlCast(const char* p) {
  return reinterpret_cast<const xmlChar*>(p);
}

// Serialize the xml tree in |root| into a vector. Consumes |root| and frees
// its memory.
std::vector<uint8_t> Serialize(UniqueXmlNodePtr root) {
  UniqueXmlDocPtr doc(xmlNewDoc(XmlCast("1.0")));
  // |doc| takes ownership of |root| here.
  xmlDocSetRootElement(doc.get(), root.release());

  xmlChar* buf;
  int length;
  xmlDocDumpFormatMemoryEnc(doc.get(), &buf, &length, "UTF-8", true);
  std::vector<uint8_t> xml(buf, buf + length);
  xmlFree(buf);

  return xml;
}

// Adds a new node with namespace |ns| called |name| as a child of |node|.
// Returns a non-owning pointer to the new node.
xmlNode* AddNode(xmlNode* node, xmlNs* ns, const std::string& name) {
  xmlNode* child = xmlNewNode(ns, XmlCast(name.c_str()));
  xmlAddChild(node, child);
  return child;
}

// Adds a new node with namespace |ns| called |name| and containing |content|
// as a child of |node|.
void AddNodeWithContent(xmlNode* node,
                        xmlNs* ns,
                        const std::string& name,
                        const std::string& content) {
  xmlNode* child = xmlNewNode(ns, XmlCast(name.c_str()));
  xmlNodeSetContent(child, XmlCast(content.c_str()));
  xmlAddChild(node, child);
}

UniqueXmlNodePtr SourceCapabilitiesAsXml(const SourceCapabilities& caps,
                                         const std::string& name,
                                         namespace_map* namespaces) {
  xmlNs* pwg = (*namespaces)[kPwgNs];
  xmlNs* scan = (*namespaces)[kScanNs];
  UniqueXmlNodePtr root(xmlNewNode(scan, XmlCast(name.c_str())));

  AddNodeWithContent(root.get(), scan, "MinWidth", "16");
  AddNodeWithContent(root.get(), scan, "MaxWidth", "2550");
  AddNodeWithContent(root.get(), scan, "MinHeight", "16");
  AddNodeWithContent(root.get(), scan, "MaxHeight", "3507");
  AddNodeWithContent(root.get(), scan, "MaxScanRegions", "1");

  xmlNode* profiles = AddNode(root.get(), scan, "SettingProfiles");
  xmlNode* profile = AddNode(profiles, scan, "SettingProfile");
  xmlNode* color_modes = AddNode(profile, scan, "ColorModes");
  for (const std::string& mode : caps.color_modes) {
    AddNodeWithContent(color_modes, scan, "ColorMode", mode);
  }
  xmlNode* document_formats = AddNode(profile, scan, "DocumentFormats");
  for (const std::string& format : caps.formats) {
    AddNodeWithContent(document_formats, pwg, "DocumentFormat", format);
  }

  xmlNode* resolutions = AddNode(profile, scan, "SupportedResolutions");
  xmlNode* discrete_resolutions =
      AddNode(resolutions, scan, "DiscreteResolutions");
  for (int resolution : caps.resolutions) {
    xmlNode* r = AddNode(discrete_resolutions, scan, "DiscreteResolution");
    AddNodeWithContent(r, scan, "XResolution", std::to_string(resolution));
    AddNodeWithContent(r, scan, "YResolution", std::to_string(resolution));
  }

  xmlNode* intents = AddNode(root.get(), scan, "SupportedIntents");
  AddNodeWithContent(intents, scan, "Intent", "Document");
  AddNodeWithContent(intents, scan, "Intent", "TextAndGraphic");
  AddNodeWithContent(intents, scan, "Intent", "Photo");
  AddNodeWithContent(intents, scan, "Intent", "Preview");

  AddNodeWithContent(root.get(), scan, "MaxOpticalXResolution", "2400");
  AddNodeWithContent(root.get(), scan, "MaxOpticalYResolution", "2400");
  AddNodeWithContent(root.get(), scan, "RiskyLeftMargin", "0");
  AddNodeWithContent(root.get(), scan, "RiskyRightMargin", "0");
  AddNodeWithContent(root.get(), scan, "RiskyTopMargin", "0");
  AddNodeWithContent(root.get(), scan, "RiskyBottomMargin", "0");

  return root;
}

}  // namespace

std::vector<uint8_t> ScannerCapabilitiesAsXml(const ScannerCapabilities& caps) {
  UniqueXmlNodePtr root(xmlNewNode(nullptr, XmlCast("ScannerCapabilities")));

  xmlNs* pwg =
      xmlNewNs(root.get(), XmlCast("http://www.pwg.org/schemas/2010/12/sm"),
               XmlCast("pwg"));
  xmlNs* scan = xmlNewNs(
      root.get(), XmlCast("http://schemas.hp.com/imaging/escl/2011/05/03"),
      XmlCast("scan"));
  xmlNewNs(root.get(), XmlCast("http://www.w3.org/2001/XMLSchema-instance"),
           XmlCast("xsi"));
  xmlSetNs(root.get(), scan);

  namespace_map namespaces;
  namespaces[kPwgNs] = pwg;
  namespaces[kScanNs] = scan;

  AddNodeWithContent(root.get(), pwg, "Version", "2.63");
  AddNodeWithContent(root.get(), pwg, "MakeAndModel", caps.make_and_model);
  AddNodeWithContent(root.get(), pwg, "SerialNumber", caps.serial_number);

  xmlNode* platen = AddNode(root.get(), scan, "Platen");
  UniqueXmlNodePtr platen_caps = SourceCapabilitiesAsXml(
      caps.platen_capabilities, "PlatenInputCaps", &namespaces);
  xmlAddChild(platen, platen_caps.release());

  return Serialize(std::move(root));
}
