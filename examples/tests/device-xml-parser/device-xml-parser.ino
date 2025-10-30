// Test XML generation for Device
#include "DLNA.h"
#include "device.h"

DLNADeviceInfo device_info;
StringRegistry strings;
XMLDeviceParser parser(device_info, strings);

void setup() {
  Serial.begin(119200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Debug);
  parser.parse((const uint8_t*) device_xml, strlen((const char*)device_xml));
}

void loop() {}
