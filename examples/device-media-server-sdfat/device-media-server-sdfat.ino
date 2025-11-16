/**
 * @file device-media-server-sdfat.ino
 * @brief DLNA Media Server example using SdFat filesystem
 *
 * This example demonstrates how to create a DLNA media server that serves
 * media files from an SD card. The SdFatContentProvider automatically scans
 * the filesystem and exposes the directory structure to DLNA clients.
 *
 * Hardware requirements:
 * - ESP32 or similar board with WiFi
 * - SD card reader connected via SPI
 * - SD card with audio/video/image files
 *
 * Configuration:
 * - Update WiFi credentials (ssid, password)
 * - Verify SD card SPI pins match your hardware
 * - Optionally customize the root path to scan
 */

#include <SdFat.h>

#include "DLNA.h"
#include "WiFi.h"
#include "tools/SdFatContentProvider.h"

// example pins for SD card
#define MISO 2
#define MOSI 15
#define SCLK 14
#define CS 13

// Replace with your WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

SdFs sd;
// Global content provider instance
SdFatContentProvider<SdFs> contentProvider;

// DLNA server configuration
const int port = 9000;
WiFiServer wifi(port);
HttpServer<WiFiClient, WiFiServer> server(wifi);
UDPService<WiFiUDP> udp;
DLNAMediaServer<WiFiClient> mediaServer(server, udp);

// Content provider instance defined above

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

  // setup SPI
  SPI.begin(SCLK, MISO, MOSI);

  if (!sd.begin(CS, SD_SCK_MHZ(4))) {
    Serial.println("SD card initialization failed!");
    Serial.println("Check:");
    Serial.println("  - SD card is inserted");
    Serial.println("  - SD card is formatted (FAT16/FAT32/exFAT)");
    Serial.println("  - CS pin is correct");
    Serial.println("  - SPI connections are correct");
    return false;
  }

  Serial.println("SD card initialized successfully");
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);  // Wait for serial port

  Serial.println("\n=== DLNA Media Server with SdFat ===\n");

  // Configure logging
  DlnaLogger.begin(Serial, DlnaLogLevel::Debug);

  // Connect to WiFi
  IPAddress local_ip = setupWifi();
  // // Configure media server
  mediaServer.setBaseURL(local_ip, port);
  mediaServer.setFriendlyName("SdFat Media Server");
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
