// Example: Device MediaServer
// Starts a minimal MediaServer device that responds to Browse requests
// with a couple of test DIDL items. Replace WiFi credentials and base URL
// as needed before running on hardware.

#include "DLNA.h"

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

DLNADeviceMgr devMgr;
WiFiClient client;
DLNAHttpRequest http(client);
UDPAsyncService udp;

// HttpServer requires a WiFiServer reference for its socket handling
WiFiServer wifi;
HttpServer server(wifi);
MediaServer mediaServer;

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("connected!");
}

// Browse callback: returns two test media items
void myBrowseCallback(const char* objectID, const char* browseFlag,
                      const char* filter, int startingIndex,
                      int requestedCount, const char* sortCriteria,
                      tiny_dlna::Vector<MediaServer::MediaItem>& results,
                      int& numberReturned, int& totalMatches, int& updateID) {
  (void)objectID; (void)browseFlag; (void)filter; (void)startingIndex;
  (void)requestedCount; (void)sortCriteria; (void)updateID;
  results.clear();

  MediaServer::MediaItem it1;
  it1.id = "1";
  it1.parentID = "0";
  it1.title = "Test Song 1";
  it1.res = "http://192.168.1.2/media/song1.mp3";
  it1.mimeType = "audio/mpeg";
  results.push_back(it1);

  MediaServer::MediaItem it2;
  it2.id = "2";
  it2.parentID = "0";
  it2.title = "Test Song 2";
  it2.res = "http://192.168.1.2/media/song2.mp3";
  it2.mimeType = "audio/mpeg";
  results.push_back(it2);

  numberReturned = (int)results.size();
  totalMatches = numberReturned;
  updateID = 1;
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();

  // set a base URL appropriate for your device/network
  mediaServer.setBaseURL("http://192.168.1.50:44757");

  // register our browse callback
  mediaServer.setBrowseCallback(myBrowseCallback);

  // setup services on the HTTP server and start SSDP/advertising
  mediaServer.setupServices(server, udp);

  // register device with manager and start (begin registers the device)
  // DLNADeviceMgr::begin(device, udp, server)
  devMgr.begin(mediaServer, udp, server);

  Serial.println("MediaServer started");
}

void loop() {
  // run device manager tasks (handles SSDP/HTTP handling internally)
  devMgr.loop();
}
