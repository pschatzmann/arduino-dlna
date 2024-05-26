// Example to test the GUPnP Network Light Test Application - we do not do any discovery hand use 

#include "DLNA.h"

const char* ssid = "";
const char* password = "";
DLNAControlPointMgr cp;
WiFiClient client;
HttpRequest http(client);
UDPAsyncService udp;
uint32_t timeout = 0;
bool current_status = false;

// login to wireless network
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
  DlnaLogger.begin(Serial, DlnaInfo);

  setupWifi();
  if (!cp.begin(http, udp, "ssdp:all", 200000, true)) {
    Serial.println("Dimmable Light not found");
    while (true);  // stop processing
  }
}

void switchLight() {
  if (millis() > timeout) {
    current_status = !current_status;
    ActionRequest action(cp.getService("SwitchPower"), "SetTarget");
    action.addArgument("newTargetValue", current_status ? "1" : "0");
    cp.addAction(action);

    auto reply = cp.executeActions();
    timeout = millis() + 1000;
  }
}

void loop() {
  cp.loop();
  switchLight();
}