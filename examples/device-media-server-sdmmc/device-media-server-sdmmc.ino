/**
 * @file device-media-server-sdmmc.ino
 * @brief DLNA Media Server example using the ESP32 SD_MMC filesystem
 *
 * This example demonstrates how to create a DLNA media server that serves
 * media files from an SD card which is connected via SD_MMC (SDIO). The
 * ESP32FSAdapter makes the ESP32 file system usable for the
 * SdFatContentProvider, which exposes the directory structure to DLNA clients.
 * The same adapter can be used for SD, LittleFS, FFat or SPIFFS.
 *
 * Hardware requirements:
 * - ESP32 board with WiFi
 * - SD card connected via SD_MMC
 * - SD card with audio/video/image files
 *
 * Configuration:
 * - Update WiFi credentials (ssid, password)
 * - Verify the SD_MMC pins match your hardware (ESP32-S3 only: the ESP32
 *   uses fixed pins)
 * - Optionally customize the root path to scan
 */

#include "FS.h"
#include "SD_MMC.h"

#include "dlna.h"
#include "WiFi.h"
#include "tools/SdFatContentProvider.h"
#include "tools/ESP32FSAdapter.h"

// example SD_MMC pins (1-bit mode) of an ESP32-S3 CAM board
#define SD_MMC_CLK 39
#define SD_MMC_CMD 38
#define SD_MMC_D0 40

// Replace with your WiFi credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";

ESP32FSAdapter sd(SD_MMC);
// Global content provider instance
SdFatContentProvider<ESP32FSAdapter> contentProvider;

// DLNA server configuration
const int port = 9000;
WiFiServer wifi(port);
HttpServer<WiFiClient, WiFiServer> server(wifi);
UDPService<WiFiUDP> udp;
DLNAMediaServer<WiFiClient> mediaServer(server, udp);

/**
 * @brief Connect to WiFi network
 */
IPAddress setupWifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  WiFi.setSleep(false);
  IPAddress ip = WiFi.localIP();
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(ip);
  return ip;
}

/**
 * @brief Initialize SD card
 * @return true if successful, false otherwise
 */
bool setupSD() {
  Serial.println("Initializing SD card...");

  // setup pins (only supported on the ESP32-S3)
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  // 1-bit mode
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD card initialization failed!");
    Serial.println("Check:");
    Serial.println("  - SD card is inserted");
    Serial.println("  - SD card is formatted (FAT16/FAT32/exFAT)");
    Serial.println("  - SD_MMC pins are correct");
    return false;
  }

  Serial.println("SD card initialized successfully");
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);  // Wait for serial port

  Serial.println("\n=== DLNA Media Server with SD_MMC ===\n");

  // Configure logging
  DlnaLogger.begin(Serial, DlnaLogLevel::Info);

  // Connect to WiFi
  IPAddress local_ip = setupWifi();
  // // Configure media server
  mediaServer.setBaseURL(local_ip, port);
  mediaServer.setFriendlyName("SD_MMC Media Server");
  mediaServer.setReference(nullptr);

  // Initialize SD card
  if (!setupSD()) {
    Serial.println("Failed to initialize SD card. Halting.");
    while (1) delay(1000);
  }

  // Initialize content provider with SD filesystem
  if (!contentProvider.begin(sd, "/")) {
    Serial.println("Failed to initialize content provider. Halting.");
    while (1) delay(1000);
  }
  Serial.print("Content provider file count: ");
  Serial.println(contentProvider.size());

  // setup file path and file handling
  contentProvider.setPath("files");
  // serve the files at /files?ID=<id> (GET, HEAD and Range requests)
  contentProvider.addFileHandler(server);

  // Wire up the content provider callbacks via lambdas calling the global
  // instance
  mediaServer.setPrepareDataCallback(
      [](const char* objectID, ContentQueryType queryType, const char* filter,
         int startingIndex, int requestedCount, const char* sortCriteria,
         int& numberReturned, int& totalMatches, int& updateID,
         void* reference) {
        contentProvider.prepareData(
            objectID, queryType, filter, startingIndex, requestedCount,
            sortCriteria, numberReturned, totalMatches, updateID, reference);
      });

  mediaServer.setGetDataCallback(
      [](int index, MediaItem& item, void* reference) -> bool {
        return contentProvider.getData(index, item, reference);
      });

  // Start the media server
  Serial.println("starting Media Server");
  if (mediaServer.begin()) {
    Serial.println("\n=== Media Server Started ===");
    Serial.print("Server URL: http://");
    Serial.print(WiFi.localIP());
    Serial.print(":");
    Serial.println(port);
    Serial.println("Ready to serve media files from SD card!");
    Serial.println("============================\n");
  } else {
    Serial.println("Failed to start media server!");
    mediaServer.logStatus();
    while (1) delay(1000);
  }

  mediaServer.logStatus();
}

void loop() { mediaServer.loop(); }
