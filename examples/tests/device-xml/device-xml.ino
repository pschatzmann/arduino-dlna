// Test XML generation for Device
#include "DLNA.h"

DLNADevice device;

void setupDevice() {
  device.setBaseURL("http:/localhost:80/test");
  device.setDeviceType("urn:schemas-upnp-org:device:MediaRenderer:1");
  device.setUDN("uuid:09349455-2941-4cf7-9847-0dd5ab210e97");
  
  auto dummyCB = [](HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
    DlnaLogger.log(DlnaLogLevel::Error, "Unhandled request: %s", requestPath);
    server->reply("text/xml", "<test/>");
  };

  auto transportCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
  server->reply("text/xml", [](Print& out){ mr_transport_xml_printer(out); });
  };

  auto connmgrCB = [](HttpServer* server, const char* requestPath,
                      HttpRequestHandlerLine* hl) {
  server->reply("text/xml", [](Print& out){ mr_connmgr_xml_printer(out); });
  };

  auto controlCB = [](HttpServer* server, const char* requestPath,
                      HttpRequestHandlerLine* hl) {
  server->reply("text/xml", [](Print& out){ mr_control_xml_printer(out); });
  };

  // define services
  DLNAServiceInfo rc, cm, avt;
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
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupDevice();
  // render device XML
  device.print(Serial);
}

void loop() {}
