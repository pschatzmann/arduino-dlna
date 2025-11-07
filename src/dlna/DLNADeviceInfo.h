
#pragma once

#include "basic/Icon.h"
#include "basic/Vector.h"
#include "dlna/Action.h"
#include "dlna/DLNAServiceInfo.h"
#include "dlna/StringRegistry.h"
#include "dlna/xml/XMLPrinter.h"
#include "http/Server/IHttpServer.h"
#include "udp/IUDPService.h"  // Ensure IUDPService is declared

namespace tiny_dlna {

/**
 * @brief Device Attributes and generation of XML using
 * urn:schemas-upnp-org:device-1-0. We could just return a predefined device xml
 * document, but we provide a dynamic generation of the service xml which should
 * be more memory efficient.
 * Strings are represented as char*, so you can assign values that are stored in
 * ProgMem to minimize the RAM usage. If you need to keep the values on the
 * heap you can use addString() method.
 * @author Phil Schatzmann
 */

class DLNADeviceInfo {
  friend class XMLDeviceParser;
  friend class DLNAControlPoint;
  template <typename>
  friend class DLNADevice;

 public:
  DLNADeviceInfo(bool ok = true) { is_active = ok; }

  // Explicitly define copy constructor as const (needed for std::vector)
  DLNADeviceInfo(const DLNADeviceInfo& other)
      : is_active(other.is_active),
        version_major(other.version_major),
        version_minor(other.version_minor),
        base_url(other.base_url),
        udn(other.udn),
        device_type(other.device_type),
        friendly_name(other.friendly_name),
        manufacturer(other.manufacturer),
        manufacturer_url(other.manufacturer_url),
        model_description(other.model_description),
        model_name(other.model_name),
        model_url(other.model_url),
        model_number(other.model_number),
        serial_number(other.serial_number),
        universal_product_code(other.universal_product_code),
        icon(other.icon),
        services(other.services),
        icons(other.icons),
        is_subcription_active(other.is_subcription_active) {}

  ~DLNADeviceInfo() { DlnaLogger.log(DlnaLogLevel::Debug, "~DLNADevice()"); }

  /// Override to initialize the device
  virtual bool begin() { return true; }

  /**
   * @brief renders the device xml into the provided Print output.
   * @param out Print target
   * @param ref optional context pointer (passed through to callbacks)
   */
  size_t print(Print& out, void* ref = nullptr) {
    XMLPrinter xp(out);
    size_t result = 0;
    result += xp.printXMLHeader();
    result += xp.printNode(
        "root",
        std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
          return ((DLNADeviceInfo*)r)->printRoot(o, r);
        }),
        this, ns);
    return result;
  }

  // sets the device type (ST or NT)
  void setDeviceType(const char* st) { device_type = st; }

  const char* getDeviceType() { return device_type; }

  /// Define the udn uuid
  void setUDN(const char* id) { udn = id; }

  /// Provide the udn uuid
  const char* getUDN() { return udn; }

  /// Defines the base url
  void setBaseURL(const char* url) {
    DlnaLogger.log(DlnaLogLevel::Info, "Base URL: %s", url);
    base_url = url;
  }

  /// Defines the base URL
  void setBaseURL(IPAddress ip, int port, const char* path = nullptr) {
    localhost = ip;
    static Str str = "http://";
    str += ip[0];
    str += ".";
    str += ip[1];
    str += ".";
    str += ip[2];
    str += ".";
    str += ip[3];
    str += ":";
    str += port;
    if (path != nullptr && !StrView(path).startsWith("/")) {
      str += "/";
    }
    str += path;
    setBaseURL(str.c_str());
  }

  /// Provides the base url
  const char* getBaseURL() {
    // replace localhost url
    if (StrView(base_url).contains("localhost")) {
      url_str = base_url;
      url_str.replace("localhost", getIPStr());
      base_url = url_str.c_str();
    }
    return base_url;
  }

  /// This method returns base url/device.xml
  Url& getDeviceURL() {
    if (!device_url) {
      Str str = getBaseURL();
      if (!str.endsWith("/")) str += "/";
      str += "device.xml";
      Url new_url(str.c_str());
      device_url = new_url;
    }
    return device_url;
  }

  /// Defines the local IP address
  void setIPAddress(IPAddress address) { localhost = address; }

  /// Provides the local IP address
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
  void setUniversalProductCode(const char* upc) {
    universal_product_code = upc;
  }
  const char* getUniversalProductCode() { return universal_product_code; }

  /// Adds a service definition
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
  /// Finds a service definition by name
  DLNAServiceInfo& getServiceByAbbrev(const char* abbrev) {
    static DLNAServiceInfo result{false};
    if (services.empty()) return result;
    for (auto& service : services) {
      if (StrView(service.subscription_namespace_abbrev).equals(abbrev)) {
        return service;
      }
    }
    return result;
  }

  /// Provides all service definitions
  Vector<DLNAServiceInfo>& getServices() { return services; }

  /// Clears all device information
  void clear() {
    services.clear();
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

  /// adds an icon
  void addIcon(Icon icon) { icons.push_back(icon); }

  /// Provides the item at indix
  Icon getIcon(int idx = 0) {
    if (icons.size() == 0) {
      Icon empty;
      return empty;
    }
    return icons[idx];
  }

  /// Sets the server to inactive
  void setActive(bool flag) { is_active = flag; }

  /// return true if active
  operator bool() { return is_active; }

  /// loop processing
  virtual bool loop() {
    delay(1);
    return true;
  }

  void setSubscriptionActive(bool flag) { is_subcription_active = flag; }

  bool isSubscriptionActive() { return is_subcription_active; }

 protected:
  bool is_active = true;
  Url device_url;
  IPAddress localhost;
  int version_major = 1;
  int version_minor = 0;
  const char* base_url = "http://localhost:9876/dlna";
  const char* udn = "uuid:09349455-2941-4cf7-9847-0dd5ab210e97";
  const char* ns = "xmlns=\"urn:schemas-upnp-org:device-1-0\"";
  const char* device_type = nullptr;
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
  Str url_str;
  bool is_subcription_active = false;

  /// to be implemented by subclasses
  virtual void setupServices(IHttpServer& server, IUDPService& udp) {}

  size_t printRoot(Print& out, void* ref) {
    XMLPrinter xp(out);
    size_t result = 0;
    result += xp.printNode(
        "specVersion",
        std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
          return ((DLNADeviceInfo*)r)->printSpecVersion(o, r);
        }),
        this);
    result += xp.printNode("URLBase", base_url);
    result += xp.printNode(
        "device",
        std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
          return ((DLNADeviceInfo*)r)->printDevice(o, r);
        }),
        this);
    return result;
  }

  size_t printDevice(Print& out, void* ref) {
    XMLPrinter xp(out);
    size_t result = 0;
    result += xp.printNode("deviceType", getDeviceType());
    result += xp.printNode("friendlyName", friendly_name);
    result += xp.printNode("manufacturer", manufacturer);
    result += xp.printNode("manufacturerURL", manufacturer_url);
    result += xp.printNode("modelDescription", model_description);
    result += xp.printNode("modelName", model_name);
    result += xp.printNode("modelNumber", model_number);
    result += xp.printNode("modelURL", model_url);
    result += xp.printNode("serialNumber", serial_number);
    result += xp.printNode("UDN", getUDN());
    result += xp.printNode("UPC", universal_product_code);
    // use ref-based callbacks that receive Print& and void*
    result += xp.printNode(
        "iconList",
        std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
          return ((DLNADeviceInfo*)r)->printIconList(o, r);
        }),
        this);
    result += xp.printNode(
        "serviceList",
        std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
          return ((DLNADeviceInfo*)r)->printServiceList(o, r);
        }),
        this);
    return result;
  }

  size_t printSpecVersion(Print& out, void* ref) {
    XMLPrinter xp(out);
    char major[5], minor[5];
    sprintf(major, "%d", this->version_major);
    sprintf(minor, "%d", this->version_minor);
    return xp.printNode("major", major) + xp.printNode("minor", minor);
  }

  size_t printServiceList(Print& out, void* ref) {
    XMLPrinter xp(out);
    size_t result = 0;
    for (auto& service : services) {
      struct Ctx {
        DLNADeviceInfo* self;
        DLNAServiceInfo* svc;
      } ctx{this, &service};
      result += xp.printNode(
          "service",
          std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
            Ctx* c = (Ctx*)r;
            return c->self->printService(o, c->svc);
          }),
          &ctx);
    }
    return result;
  }

  size_t printService(Print& out, void* srv) {
    XMLPrinter xp(out);
    size_t result = 0;
    char buffer[DLNA_MAX_URL_LEN] = {0};
    StrView url(buffer, DLNA_MAX_URL_LEN);
    DLNAServiceInfo* service = (DLNAServiceInfo*)srv;
    result += xp.printNode("serviceType", service->service_type);
    result += xp.printNode("serviceId", service->service_id);
    result +=
        xp.printNode("SCPDURL", url.buildPath(base_url, service->scpd_url));
    result += xp.printNode("controlURL",
                           url.buildPath(base_url, service->control_url));
    if (is_subcription_active)
      result += xp.printNode("eventSubURL",
                             url.buildPath(base_url, service->event_sub_url));
    else
      result += xp.printf("<eventSubURL/>");

    return result;
  }

  size_t printIconList(Print& out, void* ref) {
    // make sure we have at least the default icon
    Icon icon;
    if (icons.empty()) {
      icons.push_back(icon);
    }
    XMLPrinter xp(out);
    int result = 0;

    // print all icons
    for (auto& icon : icons) {
      struct CtxI {
        DLNADeviceInfo* self;
        Icon* icon;
      } ctx{this, &icon};
      result += xp.printNode(
          "icon",
          std::function<size_t(Print&, void*)>([](Print& o, void* r) -> size_t {
            CtxI* c = (CtxI*)r;
            return c->self->printIconDlnaInfo(o, c->icon);
          }),
          &ctx);
    }
    return result;
  }

  size_t printIconDlnaInfo(Print& out, Icon* icon) {
    XMLPrinter xp(out);
    size_t result = 0;
    if (!StrView(icon->icon_url).isEmpty()) {
      char buffer[DLNA_MAX_URL_LEN] = {0};
      StrView url(buffer, DLNA_MAX_URL_LEN);
      result += xp.printNode("mimetype", "image/png");
      result += xp.printNode("width", icon->width);
      result += xp.printNode("height", icon->height);
      result += xp.printNode("depth", icon->depth);
      result += xp.printNode("url", url.buildPath(base_url, icon->icon_url));
    }
    return result;
  }
};

}  // namespace tiny_dlna