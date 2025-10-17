// Example: ControlPointMediaRenderer
// Discovers MediaRenderer devices and demonstrates setting AVTransport URI,
// starting playback, changing volume, and querying transport state.

#include "DLNA.h"

// Replace these with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

DLNAControlPointMgr cp;
WiFiClient client;
DLNAHttpRequest http(client);
UDPAsyncService udp;
ControlPointMediaRenderer renderer(cp);

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

  // Start control point to discover devices
  if (!cp.begin(http, udp, "ssdp:all", 3000, true)) {
    Serial.println("No devices found");
    while (true) delay(1000);
  }

  Serial.print("Renderer count: ");
  Serial.println(renderer.getDeviceCount());

  if (renderer.getDeviceCount() == 0) return;

  // Example media URI. Replace with a reachable HTTP URL for your network.
  const char* mediaUri = "http://192.168.1.2/media/sample.mp3";

  Serial.println("Setting AVTransport URI...");
  if (!renderer.setAVTransportURI(mediaUri)) {
    Serial.println("Failed to set AVTransport URI");
  } else {
    Serial.println("URI set");
  }

  Serial.println("Starting playback...");
  if (!renderer.play()) {
    Serial.println("Play failed");
  } else {
    Serial.println("Play command sent");
  }

  delay(2000);

  Serial.println("Set volume to 30");
  renderer.setVolume(30);

  delay(1000);

  const char* state = renderer.getTransportState();
  Serial.print("Transport state: ");
  Serial.println(state ? state : "(unknown)");
}

void loop() {
  cp.loop();
}
