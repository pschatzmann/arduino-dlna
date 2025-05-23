
// Example for creating a new dlna device fram scratch (Media Renderer)
#include "DLNA.h"

const char* ssid = "your SSID";
const char* password = "your Password";
DLNADevice device;         // information about your device
DLNADeviceMgr device_mgr;  // basic device API
WiFiServer wifi;           // Networking Server
HttpServer server(wifi);   // Webserver
UDPAsyncService udp const char* st =
    "urn:schemas-upnp-org:device:MediaRenderer:1";
const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

void setupDeviceInfo() {
  DLNAServiceInfo rc, cm, avt;

  device.clear();
  device.setUDN(usn);
  device.setDeviceType(st);
  device.setIPAddress(WiFi.localIP());
  device.setManufacturer("Phil Schatzmann");
  device.setManufacturerURL("https://www.pschatzmann.ch/");
  device.setFriendlyName("Arduino Media Renderer");

  // // to be implemented
  // auto controlCB = [](HttpServer* server, const char* requestPath,
  //                     HttpRequestHandlerLine* hl) {
  //   DlnaLogger.log(DlnaLogLevel::Error, "Unhandled request: %s", requestPath);
  //   server->reply("text/xml", "<test/>");
  // };

  // // to be implemented
  // auto eventCB = [](HttpServer* server, const char* requestPath,
  //                   HttpRequestHandlerLine* hl) {
  //   DlnaLogger.log(DlnaLogLevel::Error, "Unhandled request: %s", requestPath);
  //   server->reply("text/xml", "<test/>");
  // };

  // provide
  auto transportCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
    server->reply("text/xml", transport_xml);
  };

  auto connmgrCB = [](HttpServer* server, const char* requestPath,
                      HttpRequestHandlerLine* hl) {
    server->reply("text/xml", connmgr_xml);
  };

  auto controlCB = [](HttpServer* server, const char* requestPath,
                      HttpRequestHandlerLine* hl) {
    server->reply("text/xml", control_xml);
  };

  // define services
  avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
            "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
            transportCB, "/AVT/control", dummyCB, "/AVT/event", dummyCB);
  cm.setup("urn:schemas-upnporg:service:ConnectionManager:1",
           "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
           connmgrCB, "/CM/control", dummyCB, "/CM/event", dummyCB);
  rc.setup("urn:schemas-upnporg:service:RenderingControl:1",
           "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
           controlCB, "/RC/control", dummyCB, "/RC/event", dummyCB);
  device.addService(rc);
  device.addService(cm);
  device.addService(avt);
}

void setup() {
  Serial.begin(115200);
  // setup logger
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);
  // start Wifi
  setupWifi();

  // provide device implementation
  setupDeviceInfo();

  // start device
  device_mgr.begin(device, udp, server);
}

void loop() { device_mgr.loop(); }
