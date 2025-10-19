#include <Arduino.h>
#include "dlna/xml/XMLParser.h"

using namespace tiny_dlna;

// Simple callback that prints events to Serial
void xmlCallback(Str& nodeName, Vector<Str>& path, Str& text, int start,
                 int len, void* ref) {
  // Build a simple path string
  Str pathStr;
  for (int i = 0; i < (int)path.size(); ++i) {
    if (i > 0) pathStr.add('/');
    pathStr.add(path[i].c_str());
  }

  Serial.print("NODE: '");
  Serial.print(nodeName.c_str());
  Serial.print("' PATH: '");
  Serial.print(pathStr.c_str());
  Serial.print("' TEXT: '");
  Serial.print(text.c_str());
  Serial.print("' [start=");
  Serial.print(start);
  Serial.print(" len=");
  Serial.print(len);
  Serial.println("]");
}

void runTest(const char* xml) {
  Serial.println("---- Test start ----");
  Serial.println(xml);
  XMLParser p(xml, xmlCallback);
  p.parse();
  Serial.println("---- Test end ----\n");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Simple element
  runTest("<root><child>Hello</child></root>");

  // Nested and attributes (attributes are ignored by the parser but should not break parsing)
  runTest("<a><b attr=\"x\">Text<b2/>More</b></a>");

  // Comments and processing instructions
  runTest("<?pi target?><root><!-- a comment --><inner>42</inner></root>");

  // Self-closing and trailing text
  runTest("<items><item/>Trailing text</items>");
}

void loop() {
  // nothing
}
