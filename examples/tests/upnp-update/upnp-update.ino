#include "DLNA.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// server config
const char* ssid = "<FILL THIS!>";
const char* password = "<FILL THIS!>";
#define LISTEN_PORT 8888              
#define LEASE_DURATION 36000  // seconds
#define FRIENDLY_NAME "FriendlyName"

UPnP tinyUPnP(20000);

void connectWiFi() {
  WiFi.disconnect();
  delay(1200);
  WiFi.mode(WIFI_STA);
  Serial.println(F("connectWiFi"));
  WiFi.begin(ssid, password);

  // wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println(F(""));
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
}

void update() {
  bool portMappingAdded = false;
  tinyUPnP.begin();
  tinyUPnP.addConfig(WiFi.localIP(), LISTEN_PORT, RULE_PROTOCOL_TCP,
                                LEASE_DURATION, FRIENDLY_NAME);
  while (!portMappingAdded) {
    portMappingAdded = tinyUPnP.save();
    Serial.println("");

    if (!portMappingAdded) {
      // for debugging, you can see this in your router too under forwarding or
      // UPnP
      for (auto &rule : tinyUPnP.listConfig()){
        Serial.println(rule.toString());
      }
      Serial.println(
          "This was printed because adding the required port mapping failed");
      delay(30000);  // 30 seconds before trying again
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("Starting..."));

  connectWiFi();
  update();
  Serial.println("UPnP done");
}

void loop(void) {
  delay(5);
  tinyUPnP.update(600000, &connectWiFi);  // 10 minutes
}