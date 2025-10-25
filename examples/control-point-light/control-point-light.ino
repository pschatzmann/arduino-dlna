// Example to test the GUPnP Network Light Test Application - we perform a DLNA
// discovery

#include "DLNA.h"

const char* ssid = "SSID";
const char* password = "PASSWORD";
DLNAControlPointMgr cp;
WiFiClient client;
DLNAHttpRequest http(client);
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
  WiFi.setSleep(false);
  Serial.println("connected!");
}

void listServices() {
  for (auto &dev : cp.getDevices()){
    Serial.print("- Device: ");
    Serial.println(dev.getDeviceType());
    for (auto &srv : dev.getServices()){
      Serial.print("  - ");
      Serial.println(srv.service_type);
    }    
  }
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();
  if (!cp.begin(http, udp, "urn:schemas-upnp-org:device:DimmableLight:1", 1000, 60*10000)) {
    Serial.println("Dimmable Light not found");
    while (true);  // stop processing
  }
  listServices();
}

void switchLight() {
  if (millis() > timeout) {
    current_status = !current_status;
    auto service = cp.getService("SwitchPower");
    if (service) {
      ActionRequest action(service, "SetTarget");
      action.addArgument("newTargetValue", current_status ? "1" : "0");
      cp.addAction(action);
    }
    auto reply = cp.executeActions();

    timeout = millis() + 1000;
  }
}

void loop() {
  cp.loop();
  switchLight();
}