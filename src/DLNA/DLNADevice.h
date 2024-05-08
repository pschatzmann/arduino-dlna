#include "DLNA/Device.h"
#include "TinyHttp/HttpServer.h"
#include "TinyHttp/Server/Url.h"
#include "UPnP/UPnP.h"

namespace tiny_dlna {

/**
 * @brief Abstract DLNADevice. The device registers itself to the network
 * and answers to the DLNA queries and requests.
 */
class DLNADevice {
 public:
  bool begin(Url baseUrl, Device& device, HttpServer& server) {
    p_server = &server;
    p_device = &device;
    base_url = baseUrl;

    // define base url
    p_device->setURLBase(baseUrl.url());

    setupServices();

    setupDLNAServer(server);

    return true;
  }

  void end() {
    for (int j = 0; j < 3; j++) {
      notifyBye();
      delay(200);
    }
    p_device->clear();
    p_server->end();
  }

  void notifyBye() {
    const char* tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: %s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: *\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:byebye\r\n"
        "USN: %s\r\n";
    snprintf(upnp.getTempBuffer(), MAX_TMP_SIZE, tmp, getMulticastAddress(),
             max_age, getNT(), getUSN());
    send(upnp.getTempBuffer());
  }

  void notifyAlive() {
    const char* tmp =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST:%s\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: *\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: %s\r\n";
    snprintf(upnp.getTempBuffer(), MAX_TMP_SIZE, tmp, getMulticastAddress(),
             max_age, getNT(), getUSN());
    send(upnp.getTempBuffer());
  }

  void sendMSearchReply() {
    const char* tmp =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = %d\r\n"
        "LOCATION: %s\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n";
    snprintf(upnp.getTempBuffer(), MAX_TMP_SIZE, tmp, max_age, base_url.url(),
             getST(), getUSN());
    send(upnp.getTempBuffer());
  }

  void loop() {
    // handle server requests
    if (p_server) p_server->doLoop();

    // update upnp
    upnp.update(1000 * 60 * 5);

    // send Alive
    if (millis() > send_alive_timeout) {
      notifyAlive();
      send_alive_timeout = millis() + (send_alive_interval_sec * 1000);
    }
  }

  Service getService(const char* id) { return p_device->getService(id); }

  void setMaxAge(int sec) { max_age = sec; }

  void setSendAliveInterval(int sec) { send_alive_interval_sec = sec; }

  const char* getUDN() { return p_device->getUDN(); }

  virtual const char* getUSN() = 0;

  virtual const char* getST() = 0;

  virtual const char* getNT() { return getST(); };

  virtual const char* getMulticastAddress() { return multicast_address; }

  Device& getDevice() { return *p_device;}

 protected:
  UPnP upnp;
  Url base_url;
  int max_age = 60 * 5;
  int send_alive_interval_sec = 10;
  uint32_t send_alive_timeout = 0;
  const char* multicast_address = "239.255.255.250:1900";
  Device* p_device = nullptr;
  HttpServer* p_server = nullptr;

  void send(const char* msg) {
    auto& udp = *upnp.getUDP();
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(msg);
    udp.endPacket();
  }

  void setupDLNAServer(HttpServer& srv) {
    auto xmlDevice = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      // auto cb = [&](Stream& out) { p_device->write(out); };
      server->reply("text/xml", "");
    };

    // device
    p_server->rewrite("/", "/device");
    p_server->rewrite("/index.html", "/device");
    p_server->on("/device", T_GET, xmlDevice);

    // Register Service URLs
    for (Service& service : p_device->getServices()) {
      p_server->on(service.scp_url, T_GET, service.scp_cb);
      p_server->on(service.control_url, T_POST, service.control_cb);
      p_server->on(service.event_sub_url, T_GET, service.event_sub_cb);
    }
  }

  const char* soapReplySuccess(const char* service, const char* name) {
    const char* tmp =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" "
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><u:%sResponse "
        "xmlns:u=\"urn:schemas-upnp-org:service:%s:1\">%s</u:%sResponse></"
        "s:Body>"
        "</s:Envelope>";
    sprintf(upnp.getTempBuffer(), tmp, service, name);
    return upnp.getTempBuffer();
  }

  const char* soapReplyDlnaError(const char* error, int errorCode) {
    const char* tmp =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<s:Fault>"
        "<faultcode>s:Client</faultcode>"
        "<faultstring>UPnPDlnaError</faultstring>"
        "<detail>"
        "<UPnPDlnaError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
        "<errorCode>%d</errorCode>"
        "<errorDescription>%s</errorDescription>"
        "</UPnPDlnaError>"
        "</detail>"
        "</s:Fault>"
        "</s:Body>"
        "</s:Envelope>";
    sprintf(upnp.getTempBuffer(), tmp, errorCode, error);
    return upnp.getTempBuffer();
  }

  virtual void setupServices() = 0;
};

/**
 * @brief MediaRenderer DLNA Device
 *
 */

class MediaRenderer : public DLNADevice {
 public:
  const char* getUSN() override { return usn; }

  const char* getST() override { return st; }

 protected:
  // Renderer, Player or Server
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "09349455-2941-4cf7-9847-1dd5ab210e97";

  void setupServices() override {
    p_device->clear();

    auto dummy = [](HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
      server->reply("text/xml", "");
    };

    // define services
    Service rc, cm, avt;
    rc.setup("RenderingControl",
             "urn:schemas-upnporg:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "RC/SCP", dummy,
             "RC/URL", dummy, "RC/EV", dummy);
    cm.setup("ConnectionManager",
             "urn:schemas-upnporg:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "CM/SCP", dummy,
             "CM/URL", dummy, "CM/EV", dummy);
    avt.setup("AVTransport", "urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "AVT/SCP", dummy, "AVT/URL",
              dummy, "AVT/EV", dummy);

    p_device->addService(rc);
    p_device->addService(cm);
    p_device->addService(avt);
  }
};

}  // namespace tiny_dlna