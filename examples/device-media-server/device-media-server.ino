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
WiFiServer wifi;
HttpServer server(wifi);
MediaServer mediaServer;

// Store items as a global const array (for PROGMEM/flash storage)
const MediaItem items[] = {
  { "1", "0", true, "Test Song 1", "http://192.168.1.2/media/song1.mp3", "audio/mpeg" },
  { "2", "0", true, "Test Song 2", "http://192.168.1.2/media/song2.mp3", "audio/mpeg" }
};
constexpr int itemCount = sizeof(items) / sizeof(items[0]);

// log into wifi
void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("connected!");
}

// PrepareData callback: sets number of items, etc.
void myPrepareData(const char* objectID, const char* browseFlag,
                   const char* filter, int startingIndex, int requestedCount,
                   const char* sortCriteria, int& numberReturned,
                   int& totalMatches, int& updateID, void* reference) {
  (void)objectID; (void)browseFlag; (void)filter; (void)startingIndex;
  (void)requestedCount; (void)sortCriteria; (void)reference;
  numberReturned = itemCount;
  totalMatches = itemCount;
  updateID = 1;
}

// GetData callback: returns each item by index
bool myGetData(int index, MediaItem& item, void* reference) {
  (void)reference;
  if (index < 0 || index >= itemCount) return false;
  item = items[index];
  return true;
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  setupWifi();

  mediaServer.setBaseURL("http://192.168.1.50:44757");
  mediaServer.setReference(nullptr); // not needed for this example
  mediaServer.setPrepareDataCallback(myPrepareData);
  mediaServer.setGetDataCallback(myGetData);
  mediaServer.setupServices(server, udp);
  
  devMgr.begin(mediaServer, udp, server);

  Serial.println("MediaServer started");
}

void loop() {
  devMgr.loop();
}
