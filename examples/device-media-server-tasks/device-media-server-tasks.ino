// Example: Device MediaServer
// Starts a minimal MediaServer device that responds to Browse requests
// with a couple of test DIDL items. Replace WiFi credentials and base URL
// as needed before running on hardware.
// URLS determined with
// http://162.55.180.156/csv/stations/search?tag=blues&offset=0&limit=30

#include "WiFi.h"
#include "dlna.h"
#include "dlna/HttpServerUsingTasks.h"

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

const int port = 9000;
WiFiServer wifi(port);
HttpServerUsingTasks<WiFiClient, WiFiServer> server(wifi);
UDPService<WiFiUDP> udp;
DLNAMediaServer<WiFiClient> mediaServer(server, udp);

// Store items as a global const array (for PROGMEM/flash storage)
// Populated from provided station list (first 30 entries). For this
const MediaItem items[] = {
  {"1", "0", true, "Radios", "", "", MediaItemClass::Folder},
  {"2", "1", true, "# RdMix Classic Rock 70s 80s 90s", "https://cast1.torontocast.com:4610/stream", "audio/mpeg", MediaItemClass::Radio},
  {"3", "1", true, "WVPE HD3 - Blues", "http://live.str3am.com:2240/live", "audio/mpeg", MediaItemClass::Radio},
  {"4", "1", true, "Blues on Radio (AAC)", "https://0n-blues.radionetz.de/0n-blues.aac", "audio/aac", MediaItemClass::Radio},
  {"5", "1", true, "Blues on Radio (MP3)", "https://0n-blues.radionetz.de/0n-blues.mp3", "audio/mpeg", MediaItemClass::Radio},
  {"6", "1", true, "[laut.fm] Freakquency (1)", "http://freakquency.stream.laut.fm/freakquency?pl=pls&t302=2025-10-26_04-06-20&uuid=612c4b16-e493-48e1-844b-55419602dd21", "audio/mpeg", MediaItemClass::Radio},
  {"7", "1", true, "[laut.fm] Freakquency (2)", "https://freakquency.stream.laut.fm/freakquency?pl=pls&t302=2025-10-26_02-15-24&uuid=70d2d17e-57aa-4ed9-a72a-18f9938c6116", "audio/mpeg", MediaItemClass::Radio},
  {"8", "1", true, "#joint radio Blues Rock", "https://betelgeuse.nucast.co.uk/stream/jrn-blues", "audio/mpeg", MediaItemClass::Radio},
  {"9", "1", true, "1.FM - Blues Radio", "http://strm112.1.fm/blues_mobile_mp3", "audio/mpeg", MediaItemClass::Radio},
  {"10", "1", true, "181.FM - True Blues", "http://listen.181fm.com/181-blues_128k.mp3", "audio/mpeg", MediaItemClass::Radio},
  {"11", "1", true, "24/7 Blues Radio", "https://ec3.yesstreaming.net:3685/stream", "audio/mpeg", MediaItemClass::Radio},
  {"12", "1", true, "24/7 Cool Radio", "https://ec3.yesstreaming.net:3665/stream", "audio/mpeg", MediaItemClass::Radio},
  {"13", "1", true, "24/7 Lounge Radio", "https://ec3.yesstreaming.net:3665/stream", "audio/mpeg", MediaItemClass::Radio},
  {"14", "1", true, "40UP Radio", "https://stream.40upradio.nl/40up", "audio/mpeg", MediaItemClass::Radio},
  {"15", "1", true, "5GTR", "http://203.45.194.8:88/broadwave.mp3", "audio/mpeg", MediaItemClass::Radio},
  {"16", "1", true, "5MBS 99.9FM", "http://stream02.sigile.net/5mbs-128-mp3", "audio/mpeg", MediaItemClass::Radio},
  {"17", "1", true, "5MBS 99.9FM (AAC 64)", "http://stream02.sigile.net/5mbs-64-aac", "audio/aac", MediaItemClass::Radio},
  {"18", "1", true, "70 80 90 Vibrazioni Rock Radio", "https://maggie.torontocast.com:2020/stream/vibrazionirockradio", "audio/mpeg", MediaItemClass::Radio},
  {"19", "1", true, "90.5 WICN Public Radio - Jazz+ for New England", "https://wicn-ice.streamguys1.com/live-mp3", "audio/mpeg", MediaItemClass::Radio},
  {"20", "1", true, "A MISSISSIPPI BLUES", "https://cast1.torontocast.com:4450/;?type=http&nocache=1745625391", "audio/mpeg", MediaItemClass::Radio},
  {"21", "1", true, "Aardvark Blues", "http://ais-edge89-dal02.cdnstream.com/b77280_128mp3", "audio/mpeg", MediaItemClass::Radio},
  {"22", "1", true, "Aardvark Blues FM", "https://ais-sa5.cdnstream1.com/b77280_128mp3", "audio/mpeg", MediaItemClass::Radio},
  {"24", "1", true, "Alive - Live Music Radio around the clock", "http://alive.stream.laut.fm/alive?t302=2025-10-26_08-06-15&uuid=5cd00d29-b101-4f23-ac53-78d61741622e", "audio/mpeg", MediaItemClass::Radio},
  {"25", "1", true, "All Blues Radio", "http://equinox.shoutca.st:8788/;", "audio/mpeg", MediaItemClass::Radio},
  {"26", "1", true, "Allzic Radio Blues", "http://allzic09.ice.infomaniak.ch/allzic09.mp3", "audio/mpeg", MediaItemClass::Radio},
  {"27", "1", true, "American Road Radio (1)", "https://c5.radioboss.fm:18224/stream", "audio/mpeg", MediaItemClass::Radio},
  {"28", "1", true, "American Road Radio (2)", "https://c5.radioboss.fm:18224/stream", "audio/mpeg", MediaItemClass::Radio},
  {"29", "1", true, "Arctic Outpost Radio", "https://radio.streemlion.com:3715/stream", "audio/mpeg", MediaItemClass::Radio},
  {"30", "1", true, "ArtSound FM", "https://stream.artsound.fm/mp3", "audio/mpeg", MediaItemClass::Radio},
  {"31", "1", true, "ArtSound FM - Canberra - 92.7 FM (MP3)", "http://stream.artsound.fm/mp3", "audio/mpeg", MediaItemClass::Radio}};
constexpr int itemCount = sizeof(items) / sizeof(items[0]);
std::vector<MediaItem> result_items;

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

/**
 * PrepareData callback: sets selects the items, etc. based on the query
 * This might by triggered by the Search or by the Browse request. BrowseMetadata
 * needs to provide the data for the indicated node and BrowseChildren needs to
 * provide all children of the indicated node.
 *
 */
void myPrepareData(const char* objectID, ContentQueryType queryType,
                   const char* filter, int startingIndex, int requestedCount,
                   const char* sortCriteria, int& numberReturned,
                   int& totalMatches, int& updateID, void* reference) {
  result_items.clear();
  switch (queryType) {
    case ContentQueryType::Search:
      for (auto& item : items) {
        if (item.itemClass != MediaItemClass::Folder)
          result_items.push_back(item);
      }
      break;
    case ContentQueryType::BrowseMetadata:
      for (auto& item : items) {
        if (StrView(item.id).equals(objectID)) result_items.push_back(item);
      }
      break;
    case ContentQueryType::BrowseChildren:
      for (auto& item : items) {
        if (StrView(item.parentID).equals(objectID))
          result_items.push_back(item);
      }
      break;
  }
  numberReturned = result_items.size();
  totalMatches = result_items.size();
}

// GetData callback: returns each item by index
bool myGetData(int index, MediaItem& item, void* reference) {
  (void)reference;
  if (index < 0 || index >= result_items.size()) return false;
  item = result_items[index];
  return true;
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Warning);

  setupWifi();

  // define local base URL based on assigned IP address
  mediaServer.setFriendlyName("ArduinoMediaServer");
  mediaServer.setBaseURL(WiFi.localIP(), port);
  mediaServer.setReference(nullptr);  // not needed for this example
  mediaServer.setPrepareDataCallback(myPrepareData);
  mediaServer.setGetDataCallback(myGetData);

  if (mediaServer.begin()) {
    Serial.println("MediaServer started");
  } else {
    Serial.println("MediaServer failed to start");
  }
}

void loop() { mediaServer.loop(); }
