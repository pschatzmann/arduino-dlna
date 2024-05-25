
// Example for creating a new dlna device fram scratch (Media Renderer)
#include "DLNA.h"

const char* ssid = "your SSID";
const char* password = "your Password";
DLNADeviceInfo deviceInfo; // information about your device
DLNADevice device;         // basic device API
WiFiServer wifi;           // Networking Server
HttpServer server(wifi);   // Webserver
UDPAsyncService udp
const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
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
  deviceInfo.clear();
  deviceInfo.setUDN(usn);
  deviceInfo.setDeviceType(st);
  deviceInfo.setIPAddress(WiFi.localIP());
  deviceInfo.setManufacturer("Phil Schatzmann");
  deviceInfo.setManufacturerURL("https://www.pschatzmann.ch/");
  deviceInfo.setFriendlyName("Arduino Media Renderer");

  // to be implemented 
  auto controlCB = [](HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaError, "Unhandled request: %s", requestPath);
    server->reply("text/xml", "<test/>");
  };

  // to be implemented 
  auto eventCB = [](HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaError, "Unhandled request: %s", requestPath);
    server->reply("text/xml", "<test/>");
  };

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
  DLNAServiceInfo rc, cm, avt;
    // avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
    //           "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
    //           transportCB, "/AVT/control", dummyCB, "/AVT/event", dummyCB);
    // cm.setup("urn:schemas-upnporg:service:ConnectionManager:1",
    //          "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
    //          connmgrCB, "/CM/control", dummyCB, "/CM/event", dummyCB);
    // rc.setup("urn:schemas-upnporg:service:RenderingControl:1",
    //          "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
    //          controlCB, "/RC/control", dummyCB, "/RC/event", dummyCB);
  deviceInfo.addService(rc);
  deviceInfo.addService(cm);
  deviceInfo.addService(avt);
}

void setup() {
  Serial.begin(115200);
  // setup logger
  DlnaLogger.begin(Serial, DlnaInfo);
  // start Wifi
  setupWifi();

  // provide device implementation
  setupDeviceInfo();

  // start device
  device.begin(deviceInfo, udp, server);
}

void loop() { device.loop(); }
