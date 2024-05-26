#pragma once

#include "Print.h"
#include "assert.h"
#include "basic/Icon.h"
#include "basic/StrView.h"
#include "dlna/DLNADevice.h"

namespace tiny_dlna {

/**
 * @brief Parses an DLNA device xml string to fill the DLNADevice data
 * structure
 * @author Phil Schatzmann
 */

class XMLDeviceParser {
 public:
  void parse(DLNADevice& result, const char* xmlStr) {
    StrView tmp(xmlStr);
    str.swap(tmp);
    p_device = &result;
    parseVersion(result);
    parseIcons(result);
    parseDevice(result);
  }

 protected:
  StrView str;
  DLNADevice* p_device = nullptr;

  /// extract string, add to string repository and return repository string
  const char* substring(char* in, int pos, int end) {
    const int len = end - pos + 1;
    char tmp[len];
    StrView tmp_str(tmp, len);
    tmp_str.substring(in, pos, end);
    return p_device->addString((char*)tmp_str.c_str());
  }

  void parseVersion(DLNADevice& result) {
    parseInt(0, "major", result.version_major);
    parseInt(0, "minor", result.version_minor);
  }

  void parseDevice(DLNADevice& result) {
    parseStr("<deviceType>", result.device_type);
    parseStr("<friendlyName>", result.friendly_name);
    parseStr("<manufacturer>", result.manufacturer);
    parseStr("<manufacturerURL>", result.manufacturer_url);
    parseStr("<modelDescription>", result.model_description);
    parseStr("<modelName>", result.model_name);
    parseStr("<modelNumber>", result.model_number);
    parseStr("<modelURL>", result.model_url);
    parseStr("<serialNumber>", result.serial_number);
    parseServices();
  };

  int parseStr(const char* name, const char*& result) {
    return parseStr(0, name, result);
  }

  int parseInt(int pos, const char* name, int& result) {
    char temp[50];
    StrView temp_view(temp, 50);
    int idx = str.indexOf(name, pos);
    int end = -1;
    if (idx > 0) {
      StrView tag(name);
      idx += tag.length();
      int end_str = str.indexOf("</", idx);
      temp_view = substring((char*)str.c_str(), pos, end_str);
      result = temp_view.toInt();
      end = str.indexOf(">", end_str);
    }
    return end;
  }

  int parseStr(int pos, const char* name, const char*& result) {
    int idx = str.indexOf(name, pos);
    int end = -1;
    if (idx > 0) {
      StrView tag(name);
      pos += tag.length();
      int end_str = str.indexOf("</", idx);
      result = substring((char*)str.c_str(), pos, end_str);
      DlnaLogger.log(DlnaInfo, "device xml %s : %s", name, result);
      end = str.indexOf(">", end_str);
    }
    return end;
  }

  // /// adds a substring
  // const char* addString(const char* str, int from, int to) {
  //   char tmp[to - from + 1] = {0};
  //   int tmp_pos = 0;
  //   for (int j = from; j < to; j++) {
  //     tmp[tmp_pos++] = str[j];
  //   }
  //   return p_device->addString(tmp);
  // }

  void parseIcons(DLNADevice& device) {
    int pos = 0;
    do {
      pos = parseIcon(device, pos);
    } while (pos > 0);
  }

  int parseIcon(DLNADevice& device, int pos) {
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

  void parseServices() {
    int pos = 0;
    do {
      pos = parseService(pos);
    } while (pos > 0);
  }

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