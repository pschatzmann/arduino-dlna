// Example for creating a Media Renderer
#include "DLNA.h"

const char* ssid = "";
const char* password = "";
MediaRenderer mr;
DLNADeviceInfo device;
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
  DlnaLogger.begin(Serial, DlnaWarning);
  setupWifi();

  // setup device: set IPAddress or BaseURL and other optional information
  device.setIPAddress(WiFi.localIP());
  device.setManufacturer("Phil Schatzmann");
  device.setManufacturerURL("https://www.pschatzmann.ch/");
  device.setFriendlyName("Arduino Media Renderer");
  mr.begin(device, udp, server);
}

void loop() { mr.loop(); }
