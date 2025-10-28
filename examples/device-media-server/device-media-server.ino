// Example: Device MediaServer
// Starts a minimal MediaServer device that responds to Browse requests
// with a couple of test DIDL items. Replace WiFi credentials and base URL
// as needed before running on hardware.
// URLS determined with http://162.55.180.156/csv/stations/search?tag=blues&offset=0&limit=30

#include "DLNA.h"

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WiFiServer wifi;
HttpServer server(wifi);
DLNADevice device;
WiFiClient client;
DLNAHttpRequest http(client);
UDPAsyncService udp;
MediaServer mediaServer;

// Store items as a global const array (for PROGMEM/flash storage)
// Populated from provided station list (first 30 entries). For this
// example we use "audio/mpeg" as the mime type for streaming URLs.
const MediaItem items[] = {
  { "0", "-1", true, "Radios", "", "", MediaItemClass::Folder },
  { "1", "0", true, "# RdMix Classic Rock 70s 80s 90s", "https://cast1.torontocast.com:4610/stream", "audio/mpeg", MediaItemClass::Radio },
  { "2", "0", true, "WVPE HD3 - Blues", "http://live.str3am.com:2240/live", "audio/mpeg", MediaItemClass::Radio },
  { "3", "0", true, "Blues on Radio (AAC)", "https://0n-blues.radionetz.de/0n-blues.aac", "audio/aac", MediaItemClass::Radio },
  { "4", "0", true, "Blues on Radio (MP3)", "https://0n-blues.radionetz.de/0n-blues.mp3", "audio/mpeg", MediaItemClass::Radio },
  { "5", "0", true, "[laut.fm] Freakquency (1)", "http://freakquency.stream.laut.fm/freakquency?pl=pls&t302=2025-10-26_04-06-20&uuid=612c4b16-e493-48e1-844b-55419602dd21", "audio/mpeg", MediaItemClass::Radio },
  { "6", "0", true, "[laut.fm] Freakquency (2)", "https://freakquency.stream.laut.fm/freakquency?pl=pls&t302=2025-10-26_02-15-24&uuid=70d2d17e-57aa-4ed9-a72a-18f9938c6116", "audio/mpeg", MediaItemClass::Radio },
  { "7", "0", true, "#joint radio Blues Rock", "https://betelgeuse.nucast.co.uk/stream/jrn-blues", "audio/mpeg", MediaItemClass::Radio },
  { "8", "0", true, "1.FM - Blues Radio", "http://strm112.1.fm/blues_mobile_mp3", "audio/mpeg", MediaItemClass::Radio },
  { "9", "0", true, "181.FM - True Blues", "http://listen.181fm.com/181-blues_128k.mp3", "audio/mpeg", MediaItemClass::Radio },
  { "10", "0", true, "24/7 Blues Radio", "https://ec3.yesstreaming.net:3685/stream", "audio/mpeg", MediaItemClass::Radio },
  { "11", "0", true, "24/7 Cool Radio", "https://ec3.yesstreaming.net:3665/stream", "audio/mpeg", MediaItemClass::Radio },
  { "12", "0", true, "24/7 Lounge Radio", "https://ec3.yesstreaming.net:3665/stream", "audio/mpeg", MediaItemClass::Radio },
  { "13", "0", true, "40UP Radio", "https://stream.40upradio.nl/40up", "audio/mpeg", MediaItemClass::Radio },
  { "14", "0", true, "5GTR", "http://203.45.194.8:88/broadwave.mp3", "audio/mpeg", MediaItemClass::Radio },
  { "15", "0", true, "5MBS 99.9FM", "http://stream02.sigile.net/5mbs-128-mp3", "audio/mpeg", MediaItemClass::Radio },
  { "16", "0", true, "5MBS 99.9FM (AAC 64)", "http://stream02.sigile.net/5mbs-64-aac", "audio/aac", MediaItemClass::Radio },
  { "17", "0", true, "70 80 90 Vibrazioni Rock Radio", "https://maggie.torontocast.com:2020/stream/vibrazionirockradio", "audio/mpeg", MediaItemClass::Radio },
  { "18", "0", true, "90.5 WICN Public Radio - Jazz+ for New England", "https://wicn-ice.streamguys1.com/live-mp3", "audio/mpeg", MediaItemClass::Radio },
  { "19", "0", true, "A MISSISSIPPI BLUES", "https://cast1.torontocast.com:4450/;?type=http&nocache=1745625391", "audio/mpeg", MediaItemClass::Radio },
  { "20", "0", true, "Aardvark Blues", "http://ais-edge89-dal02.cdnstream.com/b77280_128mp3", "audio/mpeg", MediaItemClass::Radio },
  { "21", "0", true, "Aardvark Blues FM", "https://ais-sa5.cdnstream1.com/b77280_128mp3", "audio/mpeg", MediaItemClass::Radio },
  { "23", "0", true, "Alive - Live Music Radio around the clock", "http://alive.stream.laut.fm/alive?t302=2025-10-26_08-06-15&uuid=5cd00d29-b101-4f23-ac53-78d61741622e", "audio/mpeg", MediaItemClass::Radio },
  { "24", "0", true, "All Blues Radio", "http://equinox.shoutca.st:8788/;", "audio/mpeg", MediaItemClass::Radio },
  { "25", "0", true, "Allzic Radio Blues", "http://allzic09.ice.infomaniak.ch/allzic09.mp3", "audio/mpeg", MediaItemClass::Radio },
  { "26", "0", true, "American Road Radio (1)", "https://c5.radioboss.fm:18224/stream", "audio/mpeg", MediaItemClass::Radio },
  { "27", "0", true, "American Road Radio (2)", "https://c5.radioboss.fm:18224/stream", "audio/mpeg", MediaItemClass::Radio },
  { "28", "0", true, "Arctic Outpost Radio", "https://radio.streemlion.com:3715/stream", "audio/mpeg", MediaItemClass::Radio },
  { "29", "0", true, "ArtSound FM", "https://stream.artsound.fm/mp3", "audio/mpeg", MediaItemClass::Radio },
  { "30", "0", true, "ArtSound FM - Canberra - 92.7 FM (MP3)", "http://stream.artsound.fm/mp3", "audio/mpeg", MediaItemClass::Radio }
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
  WiFi.setSleep(false);
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
  DlnaLogger.begin(Serial, DlnaLogLevel::Debug);

  setupWifi();

  // define local base URL based on assigned IP address
  mediaServer.setFriendlyName("ArduinoMediaServer");
  mediaServer.setBaseURL(WiFi.localIP(), 44757);
  mediaServer.setReference(nullptr); // not needed for this example
  mediaServer.setPrepareDataCallback(myPrepareData);
  mediaServer.setGetDataCallback(myGetData);
  
  device.begin(mediaServer, udp, server);

  Serial.println("MediaServer started");
}

void loop() {
  device.loop();
}
