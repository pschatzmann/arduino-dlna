#include "dlna.h"
#include "dlna/xml/XMLParserPrint.h"

using namespace tiny_dlna;

void printFragment(Str& node, Vector<Str>& path, Str& text, Str& attrs) {
  // Print path and node
  Serial.print("Path: ");
  for (int i = 0; i < path.size(); ++i) {
    Serial.print(path[i].c_str());
    if (i + 1 < path.size()) Serial.print("/");
  }
  Serial.print(" ");
  Serial.println(node.c_str());
  if (!text.isEmpty()) {
    Serial.print("Text: ");
    Serial.println(text.c_str());
  }
  if (!attrs.isEmpty()) {
    Serial.print("Attrs: ");
    Serial.println(attrs.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("XMLParserPrint test starting");

  XMLParserPrint xp;

  // We'll feed the XML in two parts to simulate incremental arrival.
  const char* part1 = "<root><item id=\"1\">Hello";
  const char* part2 = " World</item><item id=\"2\">Bye</item></root>";

  // write part1 and try to parse fragments repeatedly
  xp.write((const uint8_t*)part1, strlen(part1));

  Str node(40);
  Vector<Str> path{5};
  Str text(100);
  Str attrs(100);

  // parse what we can so far
  while (xp.parse(node, path, text, attrs)) {
    printFragment(node, path, text, attrs);
  }

  // now write the rest and parse remaining fragments
  xp.write((const uint8_t*)part2, strlen(part2));
  while (xp.parse(node, path, text, attrs)) {
    printFragment(node, path, text, attrs);
  }

  Serial.println("Done");
}

void loop() {
}
