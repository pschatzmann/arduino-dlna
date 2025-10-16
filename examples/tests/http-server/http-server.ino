#include "DLNA.h"

const char* ssid = "SSID";
const char* password = "PASSWORD";
WiFiServer wifi;
HttpServer server(wifi);
tiny_dlna::Url indexUrl("/index.html");

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  // connect to WIFI
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);
  setupWifi();

  const char* htmlHallo =
      "<!DOCTYPE html>"
      "<html>"
      "<body style='background-color:black; color:white'>"
      "<h1>Arduino Http Server</h1>"
      "<p>Hallo world...</p>"
      "</body>"
      "</html>";

  server.rewrite("/", "/hallo.html");
  server.rewrite("/index.html", "/hallo.html");

  server.on("/hallo.html", T_GET, "text/html", htmlHallo);
  server.begin(80);
}

void loop() { server.doLoop(); }