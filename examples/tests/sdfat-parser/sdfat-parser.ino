#include "DLNA.h"
#include "tools/SdFatParser.h"
#include "tools/TreeNode.h"
#include <SPI.h>
#include "SdFat.h"
#include <string>

// pins for audiokit
#define MISO 2
#define MOSI 15
#define SCLK 14
#define CS 13

SdFs sd;
int count = 0;
SdFatParser parser;

// output the file
void processInfo(SdFatFileInfo& info, void* ref) {
  Print *p_out = (Print*) ref;
  count++;
  // indent
  for (int j = 0; j < info.level; j++) {
    p_out->print("  ");
  }
  // print name
  p_out->println(info.name.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("starting...");

  // start SPI and setup pins
  SPI.begin(SCLK, MISO, MOSI);

  if (!sd.begin(CS, SD_SCK_MHZ(4))) {
    Serial.println("sd.begin() failed");
  }

  // setup callback
  parser.setCallback(processInfo, &Serial);
  
  auto start_ms = millis();
  
  // process ls
  sd.ls(&parser, LS_R);
  
  Serial.print("Runtime in ms: ");
  Serial.println(millis() - start_ms);
  Serial.print("Number of files: ");
  Serial.println(count);
}

void loop() {}
