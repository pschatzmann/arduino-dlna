
#pragma once

#include <functional>  // std::bind

#include "DLNAServiceInfo.h"
#include "XMLPrinter.h"
#include "basic/Vector.h"
#include "basic/Icon.h"
#include "service/Action.h"
#include "service/NamedFunction.h"
#include "vector"

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

  /// Defines the base URL
  void setFriendlyName(const char* name) { friendly_name = name; }
  void setManufacturer(const char* man) { manufacturer = man; }
  void setManufacturerURL(const char* url) { manufacturer_url = url; }
  void setModelDescription(const char* descr) { model_description = descr; }
  void setModelName(const char* name) { model_name = name; }
  void setModelNumber(const char* number) { model_number = number; }
  void setSerialNumber(const char* sn) { serial_number = sn; }
  void setUniveralProductCode(const char* upc) { universal_product_code = upc; }

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

  /// Register an generic action callback
  void setAllActionsCallback(std::function<void(Action)> cb) {
    action_callback = cb;
  }

  /// Register a callback for an action
  void addActionCallback(const char* actionName,
                         std::function<void(void)> func) {
    action_callback_vector.push_back(NamedFunction(actionName, func));
  }

  /// Overwrite the default icon
  void setIcon(Icon icn) {
    icon = icn;
  }

  Icon getIcon() {
    return icon;
  }

 protected:
  XMLPrinter xml;
  Url base_url{"http://localhost:9876/dlna"};
  Url device_url;
  IPAddress localhost;
  const char* udn = "uuid:09349455-2941-4cf7-9847-0dd5ab210e97";
  const char* ns = "xmlns=\"urn:schemas-upnp-org:device-1-0\"";
  const char* device_type = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* friendly_name = nullptr;
  const char* manufacturer = nullptr;
  const char* manufacturer_url = nullptr;
  const char* model_description = nullptr;
  const char* model_name = nullptr;
  const char* model_number = nullptr;
  const char* serial_number = nullptr;
  const char* universal_product_code = nullptr;
  Icon icon;
  Vector<DLNAServiceInfo> services;
  std::function<void(Action)> action_callback;
  Vector<NamedFunction> action_callback_vector;

  size_t printRoot() {
    size_t result = 0;
    auto printSpecVersionB = std::bind(&DLNADeviceInfo::printSpecVersion, this);
    result += xml.printNode("specVersion", printSpecVersionB);
    result += xml.printNode("URLBase", base_url.url());
    auto printDeviceB = std::bind(&DLNADeviceInfo::printDevice, this);
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
    result += xml.printNode("serialNumber", serial_number);
    result += xml.printNode("UDN", getUDN());
    result += xml.printNode("UPC", universal_product_code);
    auto printIconListCb = std::bind(&DLNADeviceInfo::printIconList, this);
    result += xml.printNode("iconList", printIconListCb);
    auto printServiceListCb =
        std::bind(&DLNADeviceInfo::printServiceList, this);
    result += xml.printNode("serviceList", printServiceListCb);
    return result;
  }

  size_t printSpecVersion() {
    return xml.printNode("major", "1") + xml.printNode("minor", "0");
  }

  size_t printServiceList() {
    size_t result = 0;
    for (auto& service : services) {
      auto printServiceCb =
          std::bind(&DLNADeviceInfo::printService, this, &service);
      result += xml.printNodeArg("service", printServiceCb);
    }
    return result;
  }

  size_t printService(void* srv) {
    size_t result = 0;
    char buffer[DLNA_MAX_URL_LEN] = {0};
    DLNAServiceInfo* service = (DLNAServiceInfo*)srv;
    result += xml.printNode("serviceType", service->service_type);
    result += xml.printNode("serviceId", service->service_id);
    result += xml.printNode("SCPDURL", getBaseUrl(service->scp_url, buffer));
    result += xml.printNode("controlURL", getBaseUrl(service->control_url, buffer));
    result += xml.printNode("eventSubURL", getBaseUrl(service->event_sub_url, buffer));
    return result;
  }

  const char* getBaseUrl(const char* url, char* buffer){
    return concat(base_url.path(), url, buffer, 200);
  }

  const char* concat(const char* prefix, const char* addr, char* buffer,
                     int len) {
    StrView str(buffer, len);
    str = prefix;
    str += addr;
    str.replace("//", "/");
    return buffer;
  }

  size_t printIconList() {
    auto printIconDlnaInfoCb =
        std::bind(&DLNADeviceInfo::printIconDlnaInfo, this);
    return xml.printNode("icon", printIconDlnaInfoCb);
  }

  size_t printIconDlnaInfo() {
    size_t result = 0;
    if (!StrView(icon.icon_url).isEmpty()) {
      result += xml.printNode("mimetype", "image/png");
      result += xml.printNode("width", icon.width);
      result += xml.printNode("height", icon.height);
      result += xml.printNode("depth", icon.depth);
      result += xml.printNode("url", icon.icon_url);
    }
    return result;
  }
};

}  // namespace tiny_dlna