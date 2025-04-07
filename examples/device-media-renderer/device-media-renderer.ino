// Example for creating a Media Renderer
#include "DLNA.h"

const char* ssid = "";
const char* password = "";
MediaRenderer media_renderer;
DLNADeviceMgr device_mgr;         // basic device API
WiFiServer wifi;
HttpServer server(wifi);
UDPAsyncService udp;


void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("connected!");
}

void setup() {
  Serial.begin(115200);
  // setup logger
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);
  setupWifi();
  media_renderer.setBaseURL("http://192.168.1.39:9999");
  // setup device: set IPAddress or BaseURL and other optional information
  device_mgr.begin(media_renderer, udp, server);
}

void loop() { device_mgr.loop(); }
