#pragma once

#include "Print.h"
#include "assert.h"
#include "basic/Icon.h"
#include "basic/StrView.h"
#include "dlna/DLNADeviceInfo.h"

namespace tiny_dlna {

/**
 * @brief Parses an DLNA device xml string to fill the DLNADevice data
 * structure
 * @author Phil Schatzmann
 */

/// @brief Parses DLNA device XML and fills DLNADeviceInfo structure
class XMLDeviceParser {
 public:
  /// @brief Parse DLNA device XML string and fill DLNADeviceInfo
  /// @param result DLNADeviceInfo to fill
  /// @param strings StringRegistry for string deduplication
  /// @param xmlStr XML string to parse
  void parse(DLNADeviceInfo& result, StringRegistry& strings, const char* xmlStr) {
    p_strings = &strings;
    StrView tmp(xmlStr);
    str.swap(tmp);
    Url empty_url;
    result.base_url = nullptr;
    result.device_url = empty_url;
    p_device = &result;
    parseVersion(result);
    parseIcons(result);
    parseDevice(result);
  }

 protected:
  /// @brief XML string view
  StrView str;
  /// @brief Pointer to device info being filled
  DLNADeviceInfo* p_device = nullptr;
  /// @brief Pointer to string registry
  StringRegistry* p_strings = nullptr;

  /// @brief Extract string, add to string repository and return repository string
  /// @param in Input string
  /// @param pos Start position
  /// @param end End position
  /// @return Pointer to string in repository
  const char* substrView(char* in, int pos, int end) {
    const int len = end - pos + 1;
    char tmp[len];
    StrView tmp_str(tmp, len);
    tmp_str.substrView(in, pos, end);
    return p_strings->add((char*)tmp_str.c_str());
  }

  /// @brief Parse version info from XML
  /// @param result DLNADeviceInfo to fill
  void parseVersion(DLNADeviceInfo& result) {
    parseInt(0, "<major>", result.version_major);
    parseInt(0, "<minor>", result.version_minor);
  }

  /// @brief Parse device info from XML
  /// @param result DLNADeviceInfo to fill
  void parseDevice(DLNADeviceInfo& result) {
    parseStr("<deviceType>", result.device_type);
    parseStr("<friendlyName>", result.friendly_name);
    parseStr("<manufacturer>", result.manufacturer);
    parseStr("<manufacturerURL>", result.manufacturer_url);
    parseStr("<modelDescription>", result.model_description);
    parseStr("<modelName>", result.model_name);
    parseStr("<modelNumber>", result.model_number);
    parseStr("<modelURL>", result.model_url);
    parseStr("<URLBase>", result.base_url);
    parseServices();
  };

  /// @brief Parse string value from XML
  /// @param name XML tag name
  /// @param result Output string pointer
  /// @return End position in XML
  int parseStr(const char* name, const char*& result) {
    return parseStr(0, name, result);
  }

  /// @brief Parse integer value from XML
  /// @param pos Start position
  /// @param name XML tag name
  /// @param result Output integer
  /// @return End position in XML
  int parseInt(int pos, const char* name, int& result) {
    char temp[50] = {0};
    StrView temp_view(temp, 50);
    int start_pos = str.indexOf(name, pos);
    int end_pos = -1;
    if (start_pos > 0) {
      StrView tag(name);
      start_pos += tag.length();
      end_pos = str.indexOf("</", start_pos);
      temp_view.substrView((char*)str.c_str(), start_pos, end_pos);
      result = temp_view.toInt();
      DlnaLogger.log(DlnaLogLevel::Debug, "device xml %s : %s / %d", name, temp_view.c_str(), result);

      end_pos = str.indexOf(">", end_pos);
    }
    return end_pos;
  }

  /// @brief Parse string value from XML at position
  /// @param pos Start position
  /// @param name XML tag name
  /// @param result Output string pointer
  /// @return End position in XML
  int parseStr(int pos, const char* name, const char*& result) {
    int start_pos = str.indexOf(name, pos);
    int end = -1;
    if (start_pos > 0) {
      StrView tag(name);
      start_pos += tag.length();
      int end_str = str.indexOf("</", start_pos);
      result = substrView((char*)str.c_str(), start_pos, end_str);
      DlnaLogger.log(DlnaLogLevel::Debug, "device xml %s : %s", name, result);
      end = str.indexOf(">", end_str);
    } else {
      result = nullptr;
    }
    return end;
  }

  /// @brief Parse icon list from XML
  /// @param device DLNADeviceInfo to fill
  void parseIcons(DLNADeviceInfo& device) {
    int pos = 0;
    do {
      pos = parseIcon(device, pos);
    } while (pos > 0);
  }

  /// @brief Parse a single icon from XML
  /// @param device DLNADeviceInfo to fill
  /// @param pos Start position
  /// @return End position in XML
  int parseIcon(DLNADeviceInfo& device, int pos) {
    Icon icon;
    int result = -1;
    int iconPos = str.indexOf("<icon>", pos);
    if (iconPos > 0) {
      parseStr(iconPos, "<mimetype>", icon.mime);
      parseInt(iconPos, "<width>", icon.width);
      parseInt(iconPos, "<height>", icon.height);
      parseInt(iconPos, "<depth>", icon.depth);
      parseStr(iconPos, "<url>", icon.icon_url);
      device.addIcon(icon);
      result = str.indexOf("</icon>", pos) + 7;
    }
    return result;
  }

  /// @brief Parse service list from XML
  void parseServices() {
    int pos = 0;
    do {
      pos = parseService(pos);
    } while (pos > 0);
  }

  /// @brief Parse a single service from XML
  /// @param pos Start position
  /// @return End position in XML
  int parseService(int pos) {
    DLNAServiceInfo service;
    int result = -1;
    int servicePos = str.indexOf("<service>", pos);
    if (servicePos > 0) {
      parseStr(servicePos, "<serviceType>", service.service_type);
      parseStr(servicePos, "<serviceId>", service.service_id);
      parseStr(servicePos, "<SCPDURL>", service.scpd_url);
      parseStr(servicePos, "<controlURL>", service.control_url);
      parseStr(servicePos, "<eventSubURL>", service.event_sub_url);
      p_device->addService(service);
      result = str.indexOf("</service>", pos) + 10;
    }
    return result;
  }
};

}  // namespace tiny_dlna