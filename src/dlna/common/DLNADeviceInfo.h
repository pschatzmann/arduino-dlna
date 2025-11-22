
#pragma once

#include "basic/Icon.h"
#include "basic/Vector.h"
#include "dlna/common/Action.h"
#include "dlna/common/DLNAServiceInfo.h"
#include "dlna/udp/IUDPService.h"  // Ensure IUDPService is declared
#include "dlna/xml/XMLPrinter.h"
#include "http/Server/IHttpServer.h"
#include "icons/icon128x128.h"
#include "icons/icon48x48.h"
#include "icons/icon512x512.h"

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

  const char* getDeviceType() { return device_type.c_str(); }

  /// Define the udn uuid
  void setUDN(const char* id) { udn = id; }

  /// Provide the udn uuid
  const char* getUDN() { return udn.c_str(); }

  /// Defines the base url
  void setBaseURL(const char* url) {
    DlnaLogger.log(DlnaLogLevel::Info, "Base URL: %s", url);
    base_url = url;
  }

  /// Defines the base URL
  void setBaseURL(IPAddress ip, int port, const char* path = nullptr) {
    localhost = ip;
    Str str{80};
    str = "http://";
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
    return base_url.c_str();
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
  const char* getNS() { return ns.c_str(); }
  void setFriendlyName(const char* name) { friendly_name = name; }
  const char* getFriendlyName() { return friendly_name.c_str(); }
  void setManufacturer(const char* man) { manufacturer = man; }
  const char* getManufacturer() { return manufacturer.c_str(); }
  void setManufacturerURL(const char* url) { manufacturer_url = url; }
  const char* getManufacturerURL() { return manufacturer_url.c_str(); }
  void setModelDescription(const char* descr) { model_description = descr; }
  const char* getModelDescription() { return model_description.c_str(); }
  void setModelName(const char* name) { model_name = name; }
  const char* getModelName() { return model_name.c_str(); }
  void setModelNumber(const char* number) { model_number = number; }
  const char* getModelNumber() { return model_number.c_str(); }
  void setSerialNumber(const char* sn) { serial_number = sn; }
  const char* getSerialNumber() { return serial_number.c_str(); }
  void setUniversalProductCode(const char* upc) {
    universal_product_code = upc;
  }
  const char* getUniversalProductCode() {
    return universal_product_code.c_str();
  }

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
    udn = "";
    ns = "";
    device_type = "";
    friendly_name = "";
    manufacturer = "";
    manufacturer_url = "";
    model_description = "";
    model_name = "";
    model_number = "";
    serial_number = "";
    universal_product_code = "";
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

  /// Provides all icons
  Vector<Icon>& getIcons() {
    if (icons.empty()) {
      // make sure we have at least the default icon
      Icon icon{"image/png",        48,          48, 24, "/icon.png",
                (uint8_t*)icon_png, icon_png_len, true};
      Icon icon128{"image/png",
                   128,
                   128,
                   24,
                   "/icon128.png",
                   (uint8_t*)icon128x128_png,
                   icon128x128_png_len};
      Icon icon512{"image/png",
                   512,
                   512,
                   24,
                   "/icon512.png",
                   (uint8_t*)icon512x512_png,
                   icon512x512_png_len};
      icons.push_back(icon);
      icons.push_back(icon128);
      icons.push_back(icon512);
    }

    return icons;
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
  bool is_active = false;
  Url device_url;
  IPAddress localhost;
  int version_major = 1;
  int version_minor = 0;
  Str base_url = "http://localhost:9876/dlna";
  Str udn = "uuid:09349455-2941-4cf7-9847-0dd5ab210e97";
  Str ns = "xmlns=\"urn:schemas-upnp-org:device-1-0\"";
  Str device_type;
  Str friendly_name;
  Str manufacturer;
  Str manufacturer_url;
  Str model_description;
  Str model_name;
  Str model_url;
  Str model_number;
  Str serial_number;
  Str universal_product_code;
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
    result += xp.printNode("SCPDURL", service->scpd_url);
    result += xp.printNode("controlURL", service->control_url);
    if (is_subcription_active)
      result += xp.printNode("eventSubURL", service->event_sub_url);
    else
      result += xp.printf("<eventSubURL/>");

    return result;
  }

  size_t printIconList(Print& out, void* ref) {
    XMLPrinter xp(out);
    int result = 0;

    // print all icons
    for (auto& icon : getIcons()) {
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
      result += xp.printNode("url", icon->icon_url);
    }
    return result;
  }
};

}  // namespace tiny_dlna