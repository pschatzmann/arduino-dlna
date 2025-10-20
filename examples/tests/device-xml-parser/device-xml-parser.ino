// Test XML generation for Device
#include "DLNA.h"
#include "device.h"

DLNADeviceInfo device_info;
XMLDeviceParser parser;
StringRegistry strings;

void setup() {
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Debug);
  parser.parse(device_info, strings, (const char*) device_xml);
}

void loop() {}
