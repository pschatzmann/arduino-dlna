
#pragma once

#include <functional>  // std::bind

#include "DLNAServiceInfo.h"
#include "StringRegistry.h"
#include "basic/Icon.h"
#include "basic/Vector.h"
#include "service/Action.h"
#include "vector"
#include "xml/XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief Device Attributes and generation of XML using
 * urn:schemas-upnp-org:device-1-0. We could just return a predefined device xml
 * document, but we provide a dynamic generation of the service xml which should
 * be more memory efficient.
 * Strings are represented as char*, so you can assign values that are stored in
 * ProgMem to mimimize the RAM useage. If you need to keep the values on the
 * heap you can use addString() method.
 * @author Phil Schatzmann
 */

class DLNADevice {
  friend class XMLDeviceParser;
  friend class DLNAControlPointMgr;

 public:
  DLNADevice(bool ok = true) { is_active = true; }
  /// renderes the device xml
  void print(Print& out) {
    xml.setOutput(out);
    xml.printXMLHeader();
    auto printRootCb = std::bind(&DLNADevice::printRoot, this);
    xml.printNode("root", printRootCb, ns);
  }

  // sets the device type (ST or NT)
  void setDeviceType(const char* st) { device_type = st; }

  const char* getDeviceType() { return device_type; }

  /// Define the udn uuid
  void setUDN(const char* id) { udn = id; }

  /// Provide the udn uuid
  const char* getUDN() { return udn; }

  /// Defines the base url
  void setBaseURL(Url url) { base_url = url; }

  /// Provides the base url
  Url& getBaseURL() {
    // replace localhost url
    if (StrView(base_url.host()).contains("localhost")) {
      String url_str;
      url_str = base_url.url();
      url_str.replace("localhost", getIPStr());
      Url new_url{url_str.c_str()};
      base_url = new_url;
    }
    return base_url;
  }

  /// This method returns base url/device.xml
  Url& getDeviceURL() {
    if (!device_url) {
      Str str = getBaseURL().url();
      if (!str.endsWith("/")) str += "/";
      str += "device.xml";
      Url new_url(str.c_str());
      device_url = new_url;
    }
    return device_url;
  }

  void setIPAddress(IPAddress address) { localhost = address; }

  IPAddress getIPAddress() { return localhost; }

  /// Provides the local address as string
  const char* getIPStr() {
    static char result[80] = {0};
    snprintf(result, 80, "%d.%d.%d.%d", localhost[0], localhost[1],
             localhost[2], localhost[3]);
    return result;
  }

  void setNS(const char* ns) { this->ns = ns; }
  const char* getNS() { return ns; }
  void setFriendlyName(const char* name) { friendly_name = name; }
  const char* getFriendlyName() { return friendly_name; }
  void setManufacturer(const char* man) { manufacturer = man; }
  const char* getManufacturer() { return manufacturer; }
  void setManufacturerURL(const char* url) { manufacturer_url = url; }
  const char* getManufacturerURL() { return manufacturer_url; }
  void setModelDescription(const char* descr) { model_description = descr; }
  const char* getModelDescription() { return model_description; }
  void setModelName(const char* name) { model_name = name; }
  const char* getModelName() { return model_name; }
  void setModelNumber(const char* number) { model_number = number; }
  const char* getModelNumber() { return model_number; }
  void setSerialNumber(const char* sn) { serial_number = sn; }
  const char* getSerialNumber() { return serial_number; }
  void setUniveralProductCode(const char* upc) { universal_product_code = upc; }
  const char* getUniveralProductCode() { return universal_product_code; }

  /// Adds a service defintion
  void addService(DLNAServiceInfo s) { services.push_back(s); }

  /// Finds a service definition by name
  DLNAServiceInfo& getService(const char* id) {
    static DLNAServiceInfo result{false};
    for (auto& service : services) {
      if (StrView(service.service_id).contains(id)) {
        return service;
      }
    }
    return result;
  }

  Vector<DLNAServiceInfo>& getServices() { return services; }

  void clear() {
    services.clear();
    strings.clear();
    udn = nullptr;
    ns = nullptr;
    device_type = nullptr;
    friendly_name = nullptr;
    manufacturer = nullptr;
    manufacturer_url = nullptr;
    model_description = nullptr;
    model_name = nullptr;
    model_number = nullptr;
    serial_number = nullptr;
    universal_product_code = nullptr;
  }

  /// Overwrite the default icon
  void clearIcons() { icons.clear(); }
  void addIcon(Icon icon) { icons.push_back(icon); }
  Icon getIcon(int idx = 0) { return icons[idx]; }

  operator bool() { return is_active; }

  /// Adds a string to the string repository
  const char* addString(char* string) { return strings.add(string); }

  /// Update the timestamp
  void updateTimestamp() { timestamp = millis(); }

  /// Returns the time when this object has been updated
  uint32_t getTimestamp() { return timestamp; }

  void setActive(bool flag) { is_active = flag; }

  StringRegistry& getStringRegistry() { return strings; }

 protected:
  uint64_t timestamp = 0;
  bool is_active = true;
  XMLPrinter xml;
  Url base_url{"http://localhost:9876/dlna"};
  Url device_url;
  IPAddress localhost;
  int version_major = 1;
  int version_minor = 0;
  const char* udn = "uuid:09349455-2941-4cf7-9847-0dd5ab210e97";
  const char* ns = "xmlns=\"urn:schemas-upnp-org:device-1-0\"";
  const char* device_type = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* friendly_name = nullptr;
  const char* manufacturer = nullptr;
  const char* manufacturer_url = nullptr;
  const char* model_description = nullptr;
  const char* model_name = nullptr;
  const char* model_url = nullptr;
  const char* model_number = nullptr;
  const char* serial_number = nullptr;
  const char* universal_product_code = nullptr;
  Icon icon;
  Vector<DLNAServiceInfo> services;
  Vector<Icon> icons;
  StringRegistry strings;

  size_t printRoot() {
    size_t result = 0;
    auto printSpecVersionB = std::bind(&DLNADevice::printSpecVersion, this);
    result += xml.printNode("specVersion", printSpecVersionB);
    result += xml.printNode("URLBase", base_url.url());
    auto printDeviceB = std::bind(&DLNADevice::printDevice, this);
    result += xml.printNode("device", printDeviceB);
    return result;
  }

  size_t printDevice() {
    size_t result = 0;
    result += xml.printNode("deviceType", getDeviceType());
    result += xml.printNode("friendlyName", friendly_name);
    result += xml.printNode("manufacturer", manufacturer);
    result += xml.printNode("manufacturerURL", manufacturer_url);
    result += xml.printNode("modelDescription", model_description);
    result += xml.printNode("modelName", model_name);
    result += xml.printNode("modelNumber", model_number);
    result += xml.printNode("modelURL", model_url);
    result += xml.printNode("serialNumber", serial_number);
    result += xml.printNode("UDN", getUDN());
    result += xml.printNode("UPC", universal_product_code);
    auto printIconListCb = std::bind(&DLNADevice::printIconList, this);
    result += xml.printNode("iconList", printIconListCb);
    auto printServiceListCb = std::bind(&DLNADevice::printServiceList, this);
    result += xml.printNode("serviceList", printServiceListCb);
    return result;
  }

  size_t printSpecVersion() {
    char major[5], minor[5];
    sprintf(major, "%d", this->version_major);
    sprintf(minor, "%d", this->version_minor);
    return xml.printNode("major", major) + xml.printNode("minor", minor);
  }

  size_t printServiceList() {
    size_t result = 0;
    for (auto& service : services) {
      auto printServiceCb =
          std::bind(&DLNADevice::printService, this, &service);
      result += xml.printNodeArg("service", printServiceCb);
    }
    return result;
  }

  size_t printService(void* srv) {
    size_t result = 0;
    char buffer[DLNA_MAX_URL_LEN] = {0};
    StrView url(buffer, DLNA_MAX_URL_LEN);
    DLNAServiceInfo* service = (DLNAServiceInfo*)srv;
    result += xml.printNode("serviceType", service->service_type);
    result += xml.printNode("serviceId", service->service_id);
    result += xml.printNode("SCPDURL",
                            url.buildPath(base_url.path(), service->scpd_url));
    result += xml.printNode(
        "controlURL", url.buildPath(base_url.path(), service->control_url));
    result += xml.printNode(
        "eventSubURL", url.buildPath(base_url.path(), service->event_sub_url));
    return result;
  }

  size_t printIconList() {
    // make sure we have at least the default icon
    Icon icon;
    if (icons.empty()) {
      icons.push_back(icon);
    }
    int result = 0;

    // print all icons
    for (auto icon : icons) {
      auto printIconDlnaInfoCb =
          std::bind(&DLNADevice::printIconDlnaInfo, this, icon);
      result += xml.printNode("icon", printIconDlnaInfoCb);
    }
    return result;
  }

  size_t printIconDlnaInfo(Icon& icon) {
    size_t result = 0;
    if (!StrView(icon.icon_url).isEmpty()) {
      char buffer[DLNA_MAX_URL_LEN] = {0};
      StrView url(buffer, DLNA_MAX_URL_LEN);
      result += xml.printNode("mimetype", "image/png");
      result += xml.printNode("width", icon.width);
      result += xml.printNode("height", icon.height);
      result += xml.printNode("depth", icon.depth);
      result +=
          xml.printNode("url", url.buildPath(base_url.path(), icon.icon_url));
    }
    return result;
  }
};

}  // namespace tiny_dlna