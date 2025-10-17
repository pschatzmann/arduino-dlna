// Test UDP receive
#include "DLNA.h"

const char *ssid = "";
const char *password = "";
UDPService<WiFiUDP> udp;
//UDPAsyncService udp;

void setupWifi() {
#ifndef IS_DESKTOP
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
#endif
}

void setup() {
  Serial.begin(114200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);
  setupWifi();
  udp.begin(DLNABroadcastAddress);
}

void loop() {
  RequestData req_data = udp.receive();
  if (req_data) {
    Serial.print(req_data.peer.toString());
    Serial.println(req_data.data.c_str());
  }
  delay(10);
}