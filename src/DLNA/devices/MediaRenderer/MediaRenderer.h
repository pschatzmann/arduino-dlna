#include "DLNA/DLNADevice.h"
#include "conmgr.h"
#include "control.h"
#include "transport.h"

namespace tiny_dlna {

/**
 * @brief MediaRenderer DLNA Device
 * @author Phil Schatzmann
 */
class MediaRenderer : public DLNADevice {
 public:
 protected:
  // Renderer, Player or Server
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "09349455-2941-4cf7-9847-1dd5ab210e97";

  void setupServices(DLNADeviceInfo& device) override {
    DlnaLogger.log(DlnaInfo,"MediaRenderer::setupServices");
    device.clear();
    device.setSerialNumber(usn);
    device.setDeviceType(st);

    auto dummyCB = [](HttpServer* server, const char* requestPath,
                    HttpRequestHandlerLine* hl) {
      DlnaLogger.log(DlnaError, "Unhandled request: %s", requestPath);                
      server->reply("text/xml", "<test/>");
    };

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
    avt.setup("AVTransport", "urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/SCP.xml", transportCB, "/AVT/CTL",
              dummyCB, "/AVT/EV", dummyCB);
    cm.setup("ConnectionManager",
             "urn:schemas-upnporg:service:ConnectionManager:1",
             "urn:upnp-org:serviceId:ConnectionManager", "/CM/SCP.xml", connmgrCB,
             "/CM/CTL", dummyCB, "/CM/EV", dummyCB);
    rc.setup("RenderingControl",
             "urn:schemas-upnporg:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/SCP.xml", controlCB,
             "/RC/CTL", dummyCB, "/RC/EV", dummyCB);

    device.addService(rc);
    device.addService(cm);
    device.addService(avt);
  }
};

}  // namespace tiny_dlna
