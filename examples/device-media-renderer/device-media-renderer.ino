// Example for creating a Media Renderer
#include "DLNA.h"

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WiFiServer wifi;
HttpServer server(wifi);
UDPAsyncService udp;
MediaRenderer media_renderer(server, udp);
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
  WiFi.setSleep(false);
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

  // start device
  if (!media_renderer.begin()) {
    Serial.println("MediaRenderer failed to start");
  }
}

void loop() { media_renderer.loop(); }
