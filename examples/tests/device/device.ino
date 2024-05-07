#include "DLNA.h"

Device device;

void setupDevice() {
  device.setURLBase("http:/localhost:80/test");

  auto dummy = [](HttpServer* server, const char* requestPath,
                  HttpRequestHandlerLine* hl) {
    server->reply("text/xml", "");
  };

  // define services
  Service rc, cm, avt;
  rc.setup("RenderingControl", "urn:schemas-upnporg:service:RenderingControl:1",
           "urn:upnp-org:serviceId:RenderingControl", "RC/SCP", dummy, "RC/URL",
           dummy, "RC/EV", dummy);
  cm.setup("ConnectionManager",
           "urn:schemas-upnporg:service:ConnectionManager:1",
           "urn:upnp-org:serviceId:ConnectionManager", "CM/SCP", dummy,
           "CM/URL", dummy, "CM/EV", dummy);
  avt.setup("AVTransport", "urn:schemas-upnp-org:service:AVTransport:1",
            "urn:upnp-org:serviceId:AVTransport", "AVT/SCP", dummy, "AVT/URL",
            dummy, "AVT/EV", dummy);

  device.addService(rc);
  device.addService(cm);
  device.addService(avt);
}

void setup() {
  Serial.begin(119200);
  
  setupDevice();
  // render device XML
  device.print(Serial);
}

void loop() {}
