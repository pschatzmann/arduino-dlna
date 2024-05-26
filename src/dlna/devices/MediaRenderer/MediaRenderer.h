#include "conmgr.h"
#include "control.h"
#include "dlna/DLNADeviceMgr.h"
#include "transport.h"

namespace tiny_dlna {

/**
 * @brief MediaRenderer DLNA Device
 * @author Phil Schatzmann
 */
class MediaRenderer : public DLNADeviceMgr {
 public:
 protected:
  // Renderer, Player or Server
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

  void setupServices(DLNADevice& device) override {
    DlnaLogger.log(DlnaInfo, "MediaRenderer::setupServices");
    device.clear();
    device.setUDN(usn);
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
};

}  // namespace tiny_dlna
