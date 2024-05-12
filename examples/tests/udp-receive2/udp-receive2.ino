// Test UDP receive test using AsyncUDP
#include "AsyncUDP.h"
#include "WiFi.h"

const char *ssid = "ssid";
const char *password = "password";
AsyncUDP udp;

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
  setupWifi();
  Serial.println("connected!");
  if (udp.listenMulticast(IPAddress(239, 255, 255, 250),
                          1900)) {  // Start listening for UDP Multicast on AP
                                    // interface only
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.printf("%lu ms:", millis());
      Serial.print("UDP Packet Received. Type: ");
      Serial.print(packet.isBroadcast()   ? "Broadcast"
                   : packet.isMulticast() ? "Multicast"
                                          : "Unicast");
      Serial.print(", Received at i/f: ");
      Serial.print(packet.interface());  // 0: STA Interface. 1: AP Interface
      Serial.print(", From: ");
      Serial.print(packet.remoteIP());
      Serial.print(":");
      Serial.print(packet.remotePort());
      Serial.print(", To: ");
      Serial.print(packet.localIP());
      Serial.print(":");
      Serial.print(packet.localPort());
      Serial.print(", Length: ");
      Serial.print(packet.length());
      Serial.println(", Data: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();
    });
  }
}

void loop() {
  // interface only
  delay(10);
}