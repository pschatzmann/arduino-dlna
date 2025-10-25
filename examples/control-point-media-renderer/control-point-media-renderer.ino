// Example: ControlPointMediaRenderer
// Discovers MediaRenderer devices and demonstrates setting AVTransport URI,
// starting playback, changing volume, and querying transport state.

#include "DLNA.h"

// Replace these with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WiFiServer wifi;
HttpServer server(wifi);
DLNAControlPointMgr cp(server); // with Notifications
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
  WiFi.setSleep(false);
  Serial.println("connected!");
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();

  // Start control point to discover devices: 10 min
  if (!renderer.begin(http, udp, 1000, 10 * 60 * 1000)) {
    Serial.println("No devices found");
    return;
  }

  // Print number of discovered renderers
  Serial.print("Renderer count: ");
  Serial.println(renderer.getDeviceCount());

  // Use first discovered renderer device
  renderer.setDeviceIndex(0);
  // Subscribe to event notifications
  renderer.subscribeNotifications(300);

  // Starting playback
  Serial.println("Starting playback...");
  if (!renderer.play("http://192.168.1.2/media/sample.mp3")) {
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

void loop() { cp.loop(); }
