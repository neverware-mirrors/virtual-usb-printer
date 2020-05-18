// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xml_util.h"

#include <memory>
#include <utility>
#include <string>

#include <libxml/tree.h>

#include <base/containers/flat_map.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

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

bool StrEqual(const xmlChar* first, const char* second) {
  return xmlStrEqual(first, XmlCast(second));
}

base::Optional<std::string> GetContent(const xmlNode* node) {
  if (!node || !node->children) {
    LOG(ERROR) << "node does not have content";
    return base::nullopt;
  }
  return reinterpret_cast<const char*>(node->children->content);
}

// Attempt to parse the contents of |node| as an integer.
base::Optional<int> GetIntContent(const xmlNode* node) {
  base::Optional<std::string> contents = GetContent(node);
  if (!contents)
    return base::nullopt;
  int value;
  if (!base::StringToInt(contents.value(), &value)) {
    LOG(ERROR) << "Failed to convert " << contents.value() << " to int";
    return base::nullopt;
  }
  return value;
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

// Serialize a map of UUIDs to JobInfo into the XML format expected for
// eSCL ScannerStatus.
// Returns a node which can be merged into the larger XML document.
UniqueXmlNodePtr JobListAsXml(const base::flat_map<std::string, JobInfo>& jobs,
                              namespace_map* namespaces) {
  xmlNs* pwg = (*namespaces)[kPwgNs];
  xmlNs* scan = (*namespaces)[kScanNs];

  UniqueXmlNodePtr root(xmlNewNode(scan, XmlCast("Jobs")));
  for (const auto& job : jobs) {
    const std::string& uuid = job.first;
    const JobInfo& info = job.second;
    xmlNode* job_info = AddNode(root.get(), scan, "JobInfo");
    AddNodeWithContent(job_info, pwg, "JobUri", "/eSCL/ScanJobs/" + uuid);
    AddNodeWithContent(job_info, pwg, "JobUuid", "urn:uuid:" + uuid);

    // Different scanners are not consistent with how they report scan job age.
    // Arbitrarily report age as elapsed seconds.
    base::TimeDelta elapsed = base::TimeTicks::Now() - info.created;
    AddNodeWithContent(job_info, scan, "Age",
                       std::to_string(elapsed.InSeconds()));

    int images_completed = 0;
    int images_to_transfer = 0;
    std::string job_state;
    // These reason strings are defined in the PWG standard. There are other
    // values possible, but for now, just use a typical value.
    std::string reason;
    switch (info.state) {
      case kPending:
        images_completed = 1;
        images_to_transfer = 1;
        job_state = "Pending";
        reason = "JobScanning";
        break;
      case kCanceled:
        images_completed = 0;
        images_to_transfer = 0;
        job_state = "Canceled";
        reason = "JobTimedOut";
        break;
      case kCompleted:
        images_completed = 1;
        images_to_transfer = 0;
        job_state = "Completed";
        reason = "JobCompletedSuccessfully";
        break;
    }

    AddNodeWithContent(job_info, pwg, "ImagesCompleted",
                       std::to_string(images_completed));
    AddNodeWithContent(job_info, pwg, "ImagesToTransfer",
                       std::to_string(images_to_transfer));
    AddNodeWithContent(job_info, pwg, "JobState", job_state);
    xmlNode* job_state_reasons = AddNode(job_info, pwg, "JobStateReasons");
    AddNodeWithContent(job_state_reasons, pwg, "JobStateReason", reason);
  }

  return root;
}

base::Optional<ScanRegion> ScanRegionFromNode(const xmlNode* node) {
  if (!node) {
    LOG(ERROR) << "ScanRegion node cannot be null";
    return base::nullopt;
  }

  ScanRegion region;
  // node->children is a null-terminated doubly-linked list of nodes. We
  // iterate over the '->next' pointers, stopping once we get a null value.
  for (xmlNode* child = node->children; child; child = child->next) {
    if (StrEqual(child->name, "ContentRegionUnits")) {
      base::Optional<std::string> content = GetContent(child);
      if (!content) {
        LOG(ERROR) << "ContentRegionUnits node is empty";
        return base::nullopt;
      }
      region.units = content.value();
    } else if (StrEqual(child->name, "Height")) {
      base::Optional<int> content = GetIntContent(child);
      if (!content) {
        LOG(ERROR) << "Height node does not have valid contents";
        return base::nullopt;
      }
      region.height = content.value();
    } else if (StrEqual(child->name, "Width")) {
      base::Optional<int> content = GetIntContent(child);
      if (!content) {
        LOG(ERROR) << "Width node does not have valid contents";
        return base::nullopt;
      }
      region.width = content.value();
    } else if (StrEqual(child->name, "XOffset")) {
      base::Optional<int> content = GetIntContent(child);
      if (!content) {
        LOG(ERROR) << "XOffset node does not have valid contents";
        return base::nullopt;
      }
      region.x_offset = content.value();
    } else if (StrEqual(child->name, "YOffset")) {
      base::Optional<int> content = GetIntContent(child);
      if (!content) {
        LOG(ERROR) << "YOffset node does not have valid contents";
        return base::nullopt;
      }
      region.y_offset = content.value();
    }
  }

  return region;
}

base::Optional<ColorMode> ColorModeFromString(const std::string& color_mode) {
  if (color_mode == "RGB24") {
    return kRGB;
  } else if (color_mode == "Grayscale8") {
    return kGrayscale;
  } else if (color_mode == "BlackAndWhite1") {
    return kBlackAndWhite;
  } else {
    return base::nullopt;
  }
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

std::vector<uint8_t> ScannerStatusAsXml(const ScannerStatus& status) {
  UniqueXmlNodePtr root(xmlNewNode(nullptr, XmlCast("ScannerStatus")));

  xmlNs* scan = xmlNewNs(
      root.get(), XmlCast("http://schemas.hp.com/imaging/escl/2011/05/03"),
      XmlCast("scan"));
  xmlNs* pwg =
      xmlNewNs(root.get(), XmlCast("http://www.pwg.org/schemas/2010/12/sm"),
               XmlCast("pwg"));
  xmlNewNs(root.get(), XmlCast("http://www.w3.org/2001/XMLSchema-instance"),
           XmlCast("xsi"));
  xmlSetNs(root.get(), scan);

  AddNodeWithContent(root.get(), pwg, "Version", "2.6.3");
  AddNodeWithContent(root.get(), pwg, "State", status.idle ? "Idle" : "Busy");

  // Add a list of all of the scan jobs
  namespace_map namespaces;
  namespaces[kPwgNs] = pwg;
  namespaces[kScanNs] = scan;
  UniqueXmlNodePtr jobs = JobListAsXml(status.jobs, &namespaces);
  xmlAddChild(root.get(), jobs.release());

  return Serialize(std::move(root));
}

base::Optional<ScanSettings> ScanSettingsFromXml(
    const std::vector<uint8_t>& xml) {
  // Since this document doesn't have a url, we use a filler value.
  const char* url = "noname.xml";
  // We don't know the encoding, so pass NULL to autodetect.
  const char* encoding = NULL;
  const UniqueXmlDocPtr doc(
      xmlReadMemory(reinterpret_cast<const char*>(xml.data()), xml.size(), url,
                    encoding, /*options=*/0));
  if (!doc) {
    LOG(ERROR) << "Could not parse data as XML document";
    return base::nullopt;
  }

  const xmlNode* root = xmlDocGetRootElement(doc.get());
  if (!root) {
    LOG(ERROR) << "XML document does not have root node";
    return base::nullopt;
  }

  ScanSettings settings;
  // Iterate over doubly-linked list of nodes in the document.
  for (const xmlNode* node = root->children; node; node = node->next) {
    if (StrEqual(node->name, "ScanRegions")) {
      // Iterate over doubly-linked list of children of the ScanRegions node.
      for (xmlNode* child = node->children; child; child = child->next) {
        if (StrEqual(child->name, "ScanRegion")) {
          base::Optional<ScanRegion> region = ScanRegionFromNode(child);
          if (!region) {
            LOG(ERROR) << "Failed to parse ScanRegion";
            return base::nullopt;
          }
          settings.regions.push_back(region.value());
        }
      }
    } else if (StrEqual(node->name, "DocumentFormat")) {
      base::Optional<std::string> format = GetContent(node);
      if (!format) {
        LOG(ERROR) << "DocumentFormat node does not have valid contents.";
        return base::nullopt;
      }
      settings.document_format = format.value();
    } else if (StrEqual(node->name, "ColorMode")) {
      base::Optional<std::string> color_mode_string = GetContent(node);
      if (!color_mode_string) {
        LOG(ERROR) << "ColorMode node does not have valid contents.";
        return base::nullopt;
      }

      base::Optional<ColorMode> color_mode =
          ColorModeFromString(color_mode_string.value());
      if (!color_mode) {
        LOG(ERROR) << "Invalid ColorMode value: " << color_mode_string.value();
        return base::nullopt;
      }
      settings.color_mode = color_mode.value();
    } else if (StrEqual(node->name, "InputSource")) {
      base::Optional<std::string> source = GetContent(node);
      if (!source) {
        LOG(ERROR) << "InputSource node does not have valid contents.";
        return base::nullopt;
      }
      settings.input_source = source.value();
    } else if (StrEqual(node->name, "XResolution")) {
      base::Optional<int> resolution = GetIntContent(node);
      if (!resolution) {
        LOG(ERROR) << "XResolution node does not have valid contents.";
        return base::nullopt;
      }
      settings.x_resolution = resolution.value();
    } else if (StrEqual(node->name, "YResolution")) {
      base::Optional<int> resolution = GetIntContent(node);
      if (!resolution) {
        LOG(ERROR) << "YResolution node does not have valid contents.";
        return base::nullopt;
      }
      settings.y_resolution = resolution.value();
    }
  }

  return settings;
}
