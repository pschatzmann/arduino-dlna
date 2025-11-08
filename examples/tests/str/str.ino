    
#include "basic/Str.h"
#include <Arduino.h>

const char* xml_data = "http://freakquency.stream.laut.fm/freakquency?pl=pls&t302=2025-10-26_04-06-20&uuid=612c4b16-e493-48e1-844b-55419602dd21";

void setup() {
  tiny_dlna::Str test_str{200};
  test_str.set(xml_data);
  test_str.replaceAll("&", "&amp;");
  arduino::Serial.println(test_str.c_str());
}

void loop() {}