// Test UDP send
// we should receive it via sudo ngrep -W byline -d any udp and host 239.255.255.250 and dst port 1900

#include "DLNA.h"

const char *ssid = "";
const char *password = "";
WiFiUDP wudp;
UDPService udp;

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

void setup() {
  Serial.begin(114200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Debug);
  setupWifi();
  udp.begin(DLNABroadcastAddress);
}

void loop() {
  Serial.println("sending...");
  const char *msg = "test";
  udp.send(DLNABroadcastAddress,(uint8_t *)msg, strlen(msg));
  delay(5000);
}