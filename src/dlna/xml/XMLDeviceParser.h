#pragma once

#include "Print.h"
#include "XMLParserPrint.h"
#include "assert.h"
#include "basic/Icon.h"
#include "basic/StrView.h"
#include "dlna/DLNADeviceInfo.h"

namespace tiny_dlna {

/**
 * @brief Incremental XML device parser using XMLParserPrint
 *
 * This parser consumes parse events from an XMLParserPrint instance and
 * fills a DLNADeviceInfo structure incrementally. It keeps internal state
 * across multiple calls so callers can feed XML data in chunks.
 */
class XMLDeviceParser {
 public:
  XMLDeviceParser(DLNADeviceInfo& result, StringRegistry& strings) {
    resetState();
    p_registry = &strings;
    p_device = &result;
  }

  /// Parse available nodes from xml_parser and populate `result`.
  /// Caller may call this repeatedly as more data is written into xml_parser.
  void parse(const uint8_t* buffer, size_t len) {
    // Feed the buffer to the XML parser

    DLNADeviceInfo& result = *p_device;
    Str node;
    Str text;
    Str attr;
    Vector<Str> path;

    xml_parser.write(buffer, len);

    // consume all parsed nodes available so far
    while (xml_parser.parse(node, path, text, attr)) {
      bool path_has_service = false;
      bool path_has_icon = false;
      for (int i = 0; i < path.size(); ++i) {
        if (path[i] == "service") path_has_service = true;
        if (path[i] == "icon") path_has_icon = true;
      }

      if (path_has_service) {
        if (!in_service) {
          in_service = true;
          cur_service = DLNAServiceInfo();
        }
        if (node.equals("serviceType")) {
          const char* t = text.c_str();
          if (t && *t) cur_service.service_type = p_registry->add(t);
        } else if (node.equals("serviceId")) {
          const char* t = text.c_str();
          if (t && *t) cur_service.service_id = p_registry->add(t);
        } else if (node.equals("SCPDURL")) {
          const char* t = text.c_str();
          if (t && *t) cur_service.scpd_url = p_registry->add(t);
        } else if (node.equals("controlURL")) {
          const char* t = text.c_str();
          if (t && *t) cur_service.control_url = p_registry->add(t);
        } else if (node.equals("eventSubURL")) {
          const char* t = text.c_str();
          if (t && *t) cur_service.event_sub_url = p_registry->add(t);
        }
      } else if (path_has_icon) {
        if (!in_icon) {
          in_icon = true;
          cur_icon = Icon();
        }
        if (node.equals("mimetype")) {
          const char* t = text.c_str();
          if (t && *t) cur_icon.mime = p_registry->add(t);
        } else if (node.equals("width")) {
          const char* t = text.c_str();
          cur_icon.width = t ? atoi(t) : 0;
        } else if (node.equals("height")) {
          const char* t = text.c_str();
          cur_icon.height = t ? atoi(t) : 0;
        } else if (node.equals("depth")) {
          const char* t = text.c_str();
          cur_icon.depth = t ? atoi(t) : 0;
        } else if (node.equals("url")) {
          const char* t = text.c_str();
          if (t && *t) cur_icon.icon_url = p_registry->add(t);
        }
      } else {
        // device-level
        if (node.equals("deviceType")) {
          const char* t = text.c_str();
          if (t && *t) result.device_type = p_registry->add(t);
        } else if (node.equals("friendlyName")) {
          const char* t = text.c_str();
          if (t && *t) result.friendly_name = p_registry->add(t);
        } else if (node.equals("manufacturer")) {
          const char* t = text.c_str();
          if (t && *t) result.manufacturer = p_registry->add(t);
        } else if (node.equals("manufacturerURL")) {
          const char* t = text.c_str();
          if (t && *t) result.manufacturer_url = p_registry->add(t);
        } else if (node.equals("modelDescription")) {
          const char* t = text.c_str();
          if (t && *t) result.model_description = p_registry->add(t);
        } else if (node.equals("modelName")) {
          const char* t = text.c_str();
          if (t && *t) result.model_name = p_registry->add(t);
        } else if (node.equals("modelNumber")) {
          const char* t = text.c_str();
          if (t && *t) result.model_number = p_registry->add(t);
        } else if (node.equals("modelURL")) {
          const char* t = text.c_str();
          if (t && *t) result.model_url = p_registry->add(t);
        } else if (node.equals("serialNumber")) {
          const char* t = text.c_str();
          if (t && *t) result.serial_number = p_registry->add(t);
        } else if (node.equals("UPC")) {
          const char* t = text.c_str();
          if (t && *t) result.universal_product_code = p_registry->add(t);
        } else if (node.equals("UDN")) {
          const char* t = text.c_str();
          if (t && *t) result.udn = p_registry->add(t);
        } else if (node.equals("URLBase")) {
          const char* t = text.c_str();
          if (t && *t) result.base_url = p_registry->add(t);
        }
      }

      // Detect leaving a service/icon block: xml_parser.parse returns nodes in
      // document order; when the current node is outside service/icon but we
      // had an open service/icon, the next non-service/icon node indicates
      // the service/icon ended.
      if (!path_has_service && in_service) {
        // flush service
        if (cur_service.service_id != nullptr ||
            cur_service.service_type != nullptr) {
          result.addService(cur_service);
        }
        cur_service = DLNAServiceInfo();
        in_service = false;
      }
      if (!path_has_icon && in_icon) {
        result.addIcon(cur_icon);
        cur_icon = Icon();
        in_icon = false;
      }
    }
  }

  /// Finalize parser state and flush any pending objects
  void finalize(DLNADeviceInfo& result) {
    if (in_service) {
      if (cur_service.service_id != nullptr ||
          cur_service.service_type != nullptr) {
        result.addService(cur_service);
      }
      in_service = false;
      cur_service = DLNAServiceInfo();
    }
    if (in_icon) {
      result.addIcon(cur_icon);
      in_icon = false;
      cur_icon = Icon();
    }
  }

 protected:
  DLNAServiceInfo cur_service;
  DLNADeviceInfo* p_device = nullptr;
  StringRegistry* p_registry = nullptr;
  XMLParserPrint xml_parser;
  bool in_service = false;
  bool in_icon = false;
  Icon cur_icon;

  void resetState() {
    p_device = nullptr;
    p_registry = nullptr;
    in_service = false;
    cur_service = DLNAServiceInfo();
    in_icon = false;
    cur_icon = Icon();
  }
};

}  // namespace tiny_dlna