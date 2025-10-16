// Example for creating a Media Renderer
#include "DLNA.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"

const char* ssid = "";
const char* password = "";
MediaRenderer media_renderer;
DLNADeviceMgr device_mgr;         // basic device API
WiFiServer wifi;
HttpServer server(wifi);
UDPAsyncService udp;
CsvOutput out(Serial);  
MultiDecoder multi_decoder;
WAVDecoder dec_wav;

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

  // setup media renderer
  multi_decoder.addDecoder(dec_wav,"audio/wav");
  media_renderer.setDecoder(multi_decoder);
  media_renderer.setBaseURL(WiFi.localIP(), 9999);
  media_renderer.setOutput(out);

  // setup device: set IPAddress or BaseURL and other optional information
  device_mgr.begin(media_renderer, udp, server);
}

void loop() { device_mgr.loop(); }
