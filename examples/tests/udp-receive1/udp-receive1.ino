// Test UDP receive -> does not work for multicast!
#include <WiFi.h>
#include <WiFiUdp.h>
#include "DLNA.h"

const char *ssid = "ssid";
const char *password = "password";
WiFiUDP udp;

void setupWifi() {
#ifndef IS_DESKTOP
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
#endif
}

void setup() {
  Serial.begin(114200);
  setupWifi();
  
  //udp.begin( 1900);
  udp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);
}

void loop() {
  int len = udp.parsePacket();
  if (len > 0) {
      Serial.print(toStr(udp.remoteIP()));
      Serial.print(":");
      Serial.println(udp.remotePort());
      Serial.print("length: ");
      Serial.println(len);
      char tmp[len + 1] = {0};
      int len1 = udp.readBytes(tmp, len);
      Serial.println(tmp);
  }
  delay(10);
}