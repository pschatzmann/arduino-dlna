// Example to test the GUPnP Network Light Test Application - we do not do any discovery

#include "DLNA.h"

const char* ssid = "SSID";
const char* password = "PASSWORD";
DLNAControlPointMgr cp;
WiFiClient client;
DLNAHttpRequest http(client);
UDPAsyncService udp;
uint32_t timeout = 0;
bool current_status = false;
const char* device_type = "urn:schemas-upnp-org:device:DimmableLight:1";
const char* baseUrl = "http://192.168.1.44:39837"; // check address and port

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

// do not discover device but just use some hardcoded values
void setupDevice() {
  DLNADeviceInfo device;
  device.setIPAddress(WiFi.localIP());
  device.setDeviceType(device_type);
  device.setBaseURL(baseUrl);
  device.setUDN("uuid:50d8318f-6ea2-4d3b-813f-712dd27d015a");

  DLNAServiceInfo service_switch;
  service_switch.control_url = "/SwitchPower/Control";
  service_switch.event_sub_url = "/SwitchPower/Events";
  service_switch.service_type = "urn:schemas-upnp-org:service:SwitchPower:1";
  service_switch.service_id = "urn:upnp-org:serviceId:SwitchPower:1";
  device.addService(service_switch);

  cp.setParseDevice(false);
  cp.addDevice(device);
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();
  setupDevice();
  if (!cp.begin(http, udp, device_type, 20000, true)) {
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