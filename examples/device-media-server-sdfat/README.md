# DLNA Media Server with SdFat Example

This example demonstrates how to create a DLNA/UPnP media server that serves media files from an SD card using the SdFat library.

## Overview

The example uses the `SdFatContentProvider` class to automatically:
- Scan the SD card filesystem and build an in-memory directory tree
- Expose the directory structure to DLNA clients (folders and files)
- Provide metadata for media files (MIME types, paths, hierarchy)
- Support browsing and searching through the media content

## Hardware Requirements

- **Microcontroller**: ESP32 or compatible board with WiFi
- **SD Card Reader**: SPI-connected SD card module
- **SD Card**: Formatted as FAT16, FAT32, or exFAT with media files

## Wiring

Connect your SPI SD card module to the ESP32 as follows (matches the sketch):

- SD VCC -> 3.3V
- SD GND -> GND
- SD SCK -> GPIO 14
- SD MISO -> GPIO 2
- SD MOSI -> GPIO 15
- SD CS -> GPIO 13 (configurable via `#define CS 13` in the sketch)

Notes:
- ESP32 logic is 3.3V. Use a 3.3V SD module or one with level shifting. Power it from 3.3V when possible.
- If you use different pins, update the pin macros in the sketch (`MISO`, `MOSI`, `SCLK`, `CS`).
- On other boards, SPI pins may differ; use your board's default SPI pins and keep CS consistent with the sketch.

## Configuration

### WiFi Settings
Update these constants in the sketch:
```cpp
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
```

### SD/SPI Pins
These are defined at the top of the sketch. Change them to match your wiring:
```cpp
#define MISO 2
#define MOSI 15
#define SCLK 14
#define CS 13
```

### Server Port
Default is 9000. Change if needed:
```cpp
const int port = 9000;
```

## Supported Media Files

The content provider automatically detects media types by extension:
- **Audio**: `.mp3`, `.aac`, `.m4a`, `.wav`, `.flac`, `.ogg`
- **Video**: Files with `video/` MIME type (extend via `TreeNode::addMimeRule`)
- **Images**: Files with `image/` MIME type

### Adding Custom MIME Types

Before calling `contentProvider.begin()`, you can add custom rules:
```cpp
TreeNode::addMimeRule(".opus", "audio/ogg; codecs=opus");
TreeNode::addMimeRule(".mp4", "video/mp4");
```

Or replace all rules:
```cpp
TreeNode::setMimeRules({
  {".mp3", "audio/mpeg"},
  {".flac", "audio/flac"},
  {".mp4", "video/mp4"},
});
```

## Usage

1. **Prepare SD Card**:
   - Format as FAT32
   - Copy media files organized in folders

2. **Upload Sketch**:
   - Update WiFi credentials
   - Flash to ESP32

3. **Monitor Serial Output**:
   - Watch for successful WiFi connection
   - Note the assigned IP address
   - Verify SD card initialization

4. **Connect DLNA Client**:
   - Use a DLNA/UPnP client app (e.g., VLC, BubbleUPnP)
   - Browse network devices
   - Select "SdFat Media Server"
   - Browse folders and play media

## Directory Structure Example

```
/
├── Music/
│   ├── Artist1/
│   │   ├── song1.mp3
│   │   └── song2.mp3
│   └── Artist2/
│       └── album.flac
├── Podcasts/
│   └── episode1.mp3
└── readme.txt
```

This structure will appear as browsable folders in DLNA clients.

## Troubleshooting

### SD Card Not Detected
- Verify wiring and CS pin
- Ensure card is formatted (FAT32 recommended)
- Try a different SD card
- Check SPI connections with a multimeter

### No Media Files Showing
- Verify files have supported extensions
- Check serial monitor for directory tree output
- Ensure files aren't corrupted
- Try adding custom MIME rules for your file types

### WiFi Connection Issues
- Double-check SSID and password
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check router settings

### DLNA Client Can't Find Server
- Ensure client and server are on same network
- Check firewall settings
- Try restarting the ESP32
- Look for the server at the IP shown in serial monitor

## Advanced Configuration

### Scanning a Subdirectory
Change the root path in `contentProvider.begin()`:
```cpp
contentProvider.begin(sd, "/Music");  // Only serve /Music folder
```

### Custom Friendly Name
```cpp
mediaServer.setFriendlyName("My Custom Media Server");
```

### Logging Level
Adjust for more/less debug information:
```cpp
DlnaLogger.begin(Serial, DlnaLogLevel::Debug);  // Verbose
DlnaLogger.begin(Serial, DlnaLogLevel::Warning);  // Minimal
```

### Debug Directory Tree
To see what files were scanned, add after SD initialization:
```cpp
Serial.println("\nDirectory structure:");
sd.ls(LS_R | LS_DATE | LS_SIZE);
Serial.println();
```

## Performance Notes

- The filesystem is scanned once at startup
- Directory tree is cached in RAM
- Large directory structures consume more memory
- For best performance, organize files in reasonably-sized folders
- The SPI bus speed affects file access (configured in SdFat)

## License

Same as the parent library (see repository root).
