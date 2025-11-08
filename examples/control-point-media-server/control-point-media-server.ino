// Example: ControlPointMediaServer usage
// Discovers MediaServer devices and performs a Browse on the root ("0").

#include "WiFi.h"
#include "DLNA.h"

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

HttpRequest<WiFiClient> http;
UDPService<WiFiUDP> udp;
DLNAControlPoint cp;  // no notifications needed
DLNAControlPointMediaServer cpms(cp, http, udp);


/// Simple callback that extracts and prints the item metadata
void printItemCallback(const char* name, const char* text,
                       const char* attributes) {
  const int buffer_size = 200;
  char buffer[buffer_size];
  StrView buffer_view(buffer, buffer_size);
  buffer_view.replaceAll("&quot;", "\"");
  buffer_view.replaceAll("&amp;", "&");

  static Str id="";
  static Str parentID="";
  StrView name_view(name);

  if (name_view == "container" || name_view == "item") {
    XMLAttributeParser::extractAttribute(attributes, "id=", buffer, buffer_size);
    id.set(buffer);
    XMLAttributeParser::extractAttribute(attributes, "parentID=", buffer, buffer_size);
    parentID.set(buffer);
  } else if (name_view == "dc:title") {
    Serial.print("Item: ID=");
    Serial.print(id.c_str());
    Serial.print(" ParentID=");
    Serial.print(parentID.c_str());
    Serial.print(" Title=");
    Serial.println(text);
  }
}

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
  DlnaLogger.begin(Serial, DlnaLogLevel::Warning);

  setupWifi();

  // Start control point and wait to find relevant devices: 10 min
  if (!cpms.begin(1000, 10 * 60 * 1000)) {
    Serial.println("No devices found");
    while (true) delay(1000);
  }

  Serial.println("Devices:");
  for (auto& d : cp.getDevices()) {
    Serial.print(" - ");
    Serial.println(d.getDeviceType());
  }

  // Optionally restrict to MediaServer devices (default) and list count
  Serial.print("MediaServer count: ");
  Serial.println(cpms.getDeviceCount());

  // Browse root on first matching MediaServer
  int numberReturned = 0, totalMatches = 0, updateID = 0;
  Serial.println("Sending Browse action (ObjectID=0)...");
  bool ok = cpms.browse(0, 10, ContentQueryType::BrowseChildren, printItemCallback, numberReturned, totalMatches,
                        updateID);
  Serial.println("Browse action completed (executeActions returned)");
  Serial.print("Browse ok: ");
  Serial.println(ok ? "yes" : "no");
  Serial.print("NumberReturned: ");
  Serial.println(numberReturned);
  Serial.print("TotalMatches: ");
  Serial.println(totalMatches);
}

void loop() { cp.loop(); }
