// Example for creating a Media Renderer
#include "DLNA.h"

const char* ssid = "";
const char* password = "";
MediaRenderer media_renderer;
DLNADeviceMgr device_mgr;         // basic device API
WiFiServer wifi;
HttpServer server(wifi);
UDPAsyncService udp;
// Use Serial as a simple output when no audio stack is present
Print& out = Serial;

void setupWifi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("connected!");
}

void setup() {
  // setup logger
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);
  // start Wifi
  setupWifi();

  // setup media renderer (use event callbacks to handle audio at app level)
  media_renderer.setBaseURL(WiFi.localIP(), 9999);
  media_renderer.setMediaEventHandler(
    [](MediaEvent ev, MediaRenderer& mr) {
      switch (ev) {
        case MediaEvent::SET_URI:
          Serial.print("Event: SET_URI ");
          if (mr.getCurrentUri()) Serial.println(mr.getCurrentUri());
          else Serial.println("(null)");
          break;
        case MediaEvent::PLAY:
          Serial.println("Event: PLAY");
          break;
        case MediaEvent::STOP:
          Serial.println("Event: STOP");
          break;
        case MediaEvent::SET_VOLUME:
          Serial.print("Event: SET_VOLUME ");
          Serial.println(mr.getVolume());
          break;
        case MediaEvent::SET_MUTE:
          Serial.print("Event: SET_MUTE ");
          Serial.println(mr.isMuted() ? 1 : 0);
          break;
        default:
          Serial.println("Event: OTHER");
      }
    });

  // setup device: set IPAddress or BaseURL and other optional information
  device_mgr.begin(media_renderer, udp, server);
}

void loop() { device_mgr.loop(); }
