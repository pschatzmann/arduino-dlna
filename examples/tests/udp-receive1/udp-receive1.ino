// Test UDP receive -> does not work for multicast!
#include <WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "ssid";
const char *password = "password";
WiFiUDP udp;

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("Connected");
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
      Serial.print(udp.remoteIP());
      Serial.print(":")
      Serial.println(udp.remotePort());
      char tmp[len + 1] = {0};
      int len = p_udp->readBytes(tmp, len);
      Serial.println(tmp);
  }
  delay(10);
}