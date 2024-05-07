
#pragma once

#include <functional>

#include "Service.h"
#include "Basic/Vector.h"
#include "XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief Print Device XML using urn:schemas-upnp-org:device-1-0"
 */

class Device {
 public:
  /// renderes the device xml
  void print(Print& out) {
    xml.setOutput(out);
    xml.printXMLHeader();
    auto printRootCb = std::bind(&Device::printRoot, this);
    xml.printNode("root", printRootCb, ns);
  }

  /// Define the udn uuid
  void setUDN(const char* id) { udn = id; }

  const char* getUDN() { return udn; }

  /// Defines the base URL
  void setURLBase(const char* url) { url_base = url; }
  void setNS(const char* ns) { this->ns = ns; }
  void setDeviceType(const char* dt) { device_type = dt; }
  void setDevieTypeName(const char* name) { device_type_name = name; }
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
  void addService(Service s) { services.push_back(s); }

  /// Finds a service definition by name
  Service getService(const char* id) {
    Service result;
    for (auto& service : services) {
      if (StrView(service.name).equals(id)) {
        return result;
      }
    }
    return result;
  }

  Vector<Service>& getServices() { return services; }

  void clear() { services.clear(); }

  const char* getDeviceType() { return device_type; }

 protected:
  XMLPrinter xml;
  const char* udn = "09349455-2941-4cf7-9847-0dd5ab210e97";
  const char* url_base = "";
  const char* ns = "xmlns=\" urn:schemas-upnp-org:device-1-0\"";
  const char* device_type = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* device_type_name = "Media Renderer";
  const char* manufacturer = "pschatzmann";
  const char* manufacturer_url = "https://pschatzmann.ch";
  const char* model_description = "n/a";
  const char* model_name = "n/a";
  const char* model_number = "00000001";
  const char* serial_number = "00000001";
  const char* universal_product_code = "n/a";
  const char* icon_mime = "image/png";
  int icon_width = 512;
  int icon_height = 512;
  int icon_depth = 8;
  const char* icon_url =
      "https://cdn-icons-png.flaticon.com/512/9973/9973171.png";
  Vector<Service> services;

  void printRoot() {
    auto printSpecVersionB = std::bind(&Device::printSpecVersion, this);
    xml.printNode("specVersion", printSpecVersionB);
    xml.printNode("URLBase", url_base);
    auto printDeviceB = std::bind(&Device::printDevice, this);
    xml.printNode("device", printDeviceB);
  }

  void printDevice() {
    xml.printNode("deviceType", getDeviceType());
    xml.printNode("friendlyName", device_type_name);
    xml.printNode("manufacturer", manufacturer);
    xml.printNode("manufacturerURL", manufacturer_url);
    xml.printNode("modelDescription", model_description);
    xml.printNode("modelName", model_name);
    xml.printNode("modelNumber", model_number);
    xml.printNode("serialNumber", serial_number);
    xml.printNode("UDN", getUDN());
    xml.printNode("UPC", universal_product_code);
    auto printIconListCb = std::bind(&Device::printIconList, this);
    xml.printNode("iconList", printIconListCb);
    auto printServiceListCb = std::bind(&Device::printServiceList, this);
    xml.printNode("serviceList", printServiceListCb);
  }

  void printSpecVersion() {
    xml.printNode("major", "1");
    xml.printNode("minor", "0");
  }

  void printServiceList() {
    for (auto& service : services) {
      auto printServiceCb = std::bind(&Device::printService, this, &service);
      xml.printNodeArg("service", printServiceCb);
    }
  }

  void printService(void* srv) {
    Service* service = (Service*)srv;
    xml.printNode("serviceType", service->service_type);
    xml.printNode("serviceId", service->service_id);
    xml.printNode("SCPDURL", service->scp_url);
    xml.printNode("controlURL", service->control_url);
    xml.printNode("eventSubURL", service->event_sub_url);
  }

  void printIconList() {
    auto printIconInfoCb = std::bind(&Device::printIconInfo, this);
    xml.printNode("icon", printIconInfoCb);
  }

  void printIconInfo() {
    xml.printNode("mimetype", "image/png");
    xml.printNode("width", icon_width);
    xml.printNode("height", icon_height);
    xml.printNode("depth", icon_depth);
    xml.printNode("url", icon_url);
  }
};

}  // namespace tiny_dlna