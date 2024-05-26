// Test XML generation for Device
#include "DLNA.h"
#include "device.h"

DLNADevice device;
XMLDeviceParser parser;
StringRegistry strings;

void setup() {
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaDebug);
  parser.parse(device, strings, (const char*) device_xml);
}

void loop() {}
