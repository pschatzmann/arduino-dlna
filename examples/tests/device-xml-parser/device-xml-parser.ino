// Test XML generation for Device
#include "DLNA.h"
#include "device.h"

DLNADevice device;
XMLDeviceParser parser;

void setup() {
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaDebug);
  parser.parse(device,(const char*) device_xml);
  Serial.println("Parsing done");
}

void loop() {}
