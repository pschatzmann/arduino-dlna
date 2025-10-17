// Example: ControlPointMediaServer usage
// Discovers MediaServer devices and performs a Browse on the root ("0").

#include "DLNA.h"

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

DLNAControlPointMgr cp;
WiFiClient client;
DLNAHttpRequest http(client);
UDPAsyncService udp;
ControlPointMediaServer cpms(cp);

// Simple callback that prints item metadata
void printItemCallback(const tiny_dlna::MediaItem& item, void* ref) {
  (void)ref;
  Serial.print("Item ID: ");
  Serial.println(item.id ? item.id : "(null)");
  Serial.print(" Title: ");
  Serial.println(item.title ? item.title : "(no title)");
  Serial.print(" Resource: ");
  Serial.println(item.res ? item.res : "(no res)");
  Serial.println("---");
}

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("connected!");
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();

  // Start control point and wait briefly to collect devices
  if (!cp.begin(http, udp, "ssdp:all", 3000, true)) {
    Serial.println("No devices found");
    while (true) delay(1000);
  }

  Serial.println("Devices:");
  for (auto &d : cp.getDevices()) {
    Serial.print(" - ");
    Serial.println(d.getDeviceType());
  }

  // Optionally restrict to MediaServer devices (default) and list count
  Serial.print("MediaServer count: ");
  Serial.println(cpms.getDeviceCount());

  // Browse root on first matching MediaServer
  int numberReturned = 0, totalMatches = 0, updateID = 0;
  bool ok = cpms.browse("0", "BrowseDirectChildren", 0, 10, printItemCallback,
                        numberReturned, totalMatches, updateID);
  Serial.print("Browse ok: "); Serial.println(ok ? "yes" : "no");
  Serial.print("NumberReturned: "); Serial.println(numberReturned);
  Serial.print("TotalMatches: "); Serial.println(totalMatches);
}

void loop() {
  cp.loop();
}
