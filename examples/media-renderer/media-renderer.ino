// Example for the ESP32 using the WiFi functionality 
#include "DLNA.h"

const char* ssid = "";
const char* password = "";
MediaRenderer mr;
DLNADeviceInfo device;
WiFiServer wifi;
HttpServer server(wifi);
WiFiUDP wifiUDP;
UDPService udpService(wifiUDP);

void setupWifi(){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  // setup device: set IPAddress or BaseURL and other optional information
  device.setIPAddress(WiFi.localIP());
  device.setManufacturer("Phil Schatzmann");
  device.setManufacturerURL("https://www.pschatzmann.ch/");
  mr.begin(device, udpService, server);
}

void loop() {
  mr.loop();
}
