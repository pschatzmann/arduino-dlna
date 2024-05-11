
#pragma once

#include <functional> // std::bind
#include "Basic/Vector.h"
#include "DLNAServiceInfo.h"
#include "XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief Device Attributes and generation of XML using
 * urn:schemas-upnp-org:device-1-0. We could just reutrn a predefined device xml
 * document, but we provide a dynamic generation of the service xml which should
 * be more memory efficient.
 * @author Phil Schatzmann
 */

class DLNADeviceInfo {
 public:
  /// renderes the device xml
  void print(Print& out) {
    xml.setOutput(out);
    xml.printXMLHeader();
    auto printRootCb = std::bind(&DLNADeviceInfo::printRoot, this);
    xml.printNode("root", printRootCb, ns);
  }

  // sets the device type (ST or NT)
  void setDeviceType(const char* st) { device_type = st; }

  const char* getDeviceType() { return device_type; }

  /// Define the udn uuid
  void setUDN(const char* id) { udn = id; }

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

  /// Defines the base URL
  void setFriendlyName(const char* name) { friendly_name = name; }
  void setManufacturer(const char* man) { manufacturer = man; }
  void setManufacturerURL(const char* url) { manufacturer_url = url; }
  void setModelDescription(const char* descr) { model_description = descr; }
  void setModelName(const char* name) { model_name = name; }
  void setModelNumber(const char* number) { model_number = number; }
  void setSerialNumber(const char* sn) { serial_number = sn; }
  void setUniveralProductCode(const char* upc) { universal_product_code = upc; }
  void setIconMime(const char* mime) { icon_mime = mime; }
  void setIconWidth(int width) { icon_width = width; }
  void setIconHeight(int height) { icon_height = height; }
  void setIconDepth(int depth) { icon_depth = depth; }
  void setIconURL(const char* url) { icon_url = url; }

  /// Adds a service defintion
  void addService(DLNAServiceInfo s) { services.push_back(s); }

  /// Finds a service definition by name
  DLNAServiceInfo getService(const char* id) {
    DLNAServiceInfo result;
    for (auto& service : services) {
      if (StrView(service.name).equals(id)) {
        return result;
      }
    }
    return result;
  }

  Vector<DLNAServiceInfo>& getServices() { return services; }

  void clear() { services.clear(); }

 protected:
  XMLPrinter xml;
  Url base_url{"http://localhost:80/dlna"};
  Url device_url;
  IPAddress localhost;
  const char* udn = "09349455-2941-4cf7-9847-0dd5ab210e97";
  const char* ns = "xmlns=\" urn:schemas-upnp-org:device-1-0\"";
  const char* device_type = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* friendly_name = nullptr;
  const char* manufacturer = nullptr;
  const char* manufacturer_url = nullptr;
  const char* model_description = nullptr;
  const char* model_name = nullptr;
  const char* model_number = nullptr;
  const char* serial_number = nullptr;
  const char* universal_product_code = nullptr;
  const char* icon_mime = "image/png";
  int icon_width = 512;
  int icon_height = 512;
  int icon_depth = 8;
  const char* icon_url =
      "https://cdn-icons-png.flaticon.com/512/9973/9973171.png";
  Vector<DLNAServiceInfo> services;

  void printRoot() {
    auto printSpecVersionB = std::bind(&DLNADeviceInfo::printSpecVersion, this);
    xml.printNode("specVersion", printSpecVersionB);
    xml.printNode("URLBase", base_url.url());
    auto printDeviceB = std::bind(&DLNADeviceInfo::printDevice, this);
    xml.printNode("device", printDeviceB);
  }

  void printDevice() {
    xml.printNode("deviceType", getDeviceType());
    xml.printNode("friendlyName", friendly_name);
    xml.printNode("manufacturer", manufacturer);
    xml.printNode("manufacturerURL", manufacturer_url);
    xml.printNode("modelDescription", model_description);
    xml.printNode("modelName", model_name);
    xml.printNode("modelNumber", model_number);
    xml.printNode("serialNumber", serial_number);
    xml.printNode("UDN", getUDN());
    xml.printNode("UPC", universal_product_code);
    auto printIconListCb = std::bind(&DLNADeviceInfo::printIconList, this);
    xml.printNode("iconList", printIconListCb);
    auto printServiceListCb =
        std::bind(&DLNADeviceInfo::printServiceList, this);
    xml.printNode("serviceList", printServiceListCb);
  }

  void printSpecVersion() {
    xml.printNode("major", "1");
    xml.printNode("minor", "0");
  }

  void printServiceList() {
    for (auto& service : services) {
      auto printServiceCb =
          std::bind(&DLNADeviceInfo::printService, this, &service);
      xml.printNodeArg("service", printServiceCb);
    }
  }

  void printService(void* srv) {
    DLNAServiceInfo* service = (DLNAServiceInfo*)srv;
    xml.printNode("serviceType", service->service_type);
    xml.printNode("serviceId", service->service_id);
    xml.printNode("SCPDURL", service->scp_url);
    xml.printNode("controlURL", service->control_url);
    xml.printNode("eventSubURL", service->event_sub_url);
  }

  void printIconList() {
    auto printIconDlnaInfoCb =
        std::bind(&DLNADeviceInfo::printIconDlnaInfo, this);
    xml.printNode("icon", printIconDlnaInfoCb);
  }

  void printIconDlnaInfo() {
    if (!StrView(icon_url).isEmpty()) {
      xml.printNode("mimetype", "image/png");
      xml.printNode("width", icon_width);
      xml.printNode("height", icon_height);
      xml.printNode("depth", icon_depth);
      xml.printNode("url", icon_url);
    }
  }
};

}  // namespace tiny_dlna