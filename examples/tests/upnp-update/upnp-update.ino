#include "DLNA.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// server config
const char* ssid = "<FILL THIS!>";
const char* password = "<FILL THIS!>";
#define LISTEN_PORT 8888              
#define LEASE_DURATION 36000  // seconds
#define FRIENDLY_NAME "FriendlyName"

UPnP upnp(20000);

void connectWiFi() {
#ifndef IS_DESKTOP
  WiFi.disconnect();
  delay(1200);
  WiFi.mode(WIFI_STA);
  Serial.println(F("connectWiFi"));
  WiFi.begin(ssid, password);

  // wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#else
  upnp.setLocalIP(IPAddress(192,168,1,35));
  upnp.setGatewayIP(IPAddress(192,168,1,1));
#endif
}

void update() {
  bool portMappingAdded = false;
  upnp.begin();
  upnp.addConfig(LISTEN_PORT, RULE_PROTOCOL_TCP,
                                LEASE_DURATION, FRIENDLY_NAME);
  while (!portMappingAdded) {
    portMappingAdded = upnp.save();
    Serial.println("");

    if (!portMappingAdded) {
      // for debugging, you can see this in your router too under forwarding or
      // UPnP
      for (auto &rule : upnp.listConfig()){
        Serial.println(rule.toString().c_str());
      }
      Serial.println(
          "This was printed because adding the required port mapping failed");
      delay(30000);  // 30 seconds before trying again
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaInfo);
  Serial.println(F("Starting..."));

  connectWiFi();
  update();
  Serial.println("UPnP done");
}

void loop(void) {
  delay(5);
  upnp.update(600000, &connectWiFi);  // 10 minutes
}