// This example will list all the SSDP devices in the local network

#include "DLNA.h"
#include "WiFi.h"

const char* ssid = "<FILL THIS!>";
const char* password = "<FILL THIS!>";

UPnP tinyUPnP(12000);  // when using the library for listing SSDP devices, a
                       // timeout must be set

void connectWiFi() {
#ifndef IS_DESKTOP  
  WiFi.disconnect();
  delay(1200);
  WiFi.mode(WIFI_STA);
  // WiFi.setAutoConnect(true);
  Serial.println("connectWiFi");
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
  Serial.print("Gateway Address: ");
  Serial.println(WiFi.gatewayIP().toString());
  Serial.print("Network Mask: ");
  Serial.println(WiFi.subnetMask().toString());
#endif
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("Starting...");

  connectWiFi();
  
  tinyUPnP.begin();
  for (auto &dev : tinyUPnP.listDevices()){
    Serial.println(dev.toString().c_str());
  }
}

void loop(void) { delay(100000); }