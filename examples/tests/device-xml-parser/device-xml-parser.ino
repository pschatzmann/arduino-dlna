// Test XML generation for Device
#include "DLNA.h"
#include "device.h"

DLNADevice device;
XMLDeviceParser parser;

void setup() {
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaDebug);
  parser.parse(device,(const char*) device_xml);
  Serial.print("Parsing done with total string bytes ");
  Serial.println(device.getStringRegistry().size());
  device.clear();
  assert(device.getStringRegistry().size() ==0);
  Serial.println("---end---");
}

void loop() {}
