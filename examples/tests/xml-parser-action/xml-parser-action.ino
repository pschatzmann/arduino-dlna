#include "DLNA.h"
#include "dlna/xml/XMLParserPrint.h"

using namespace tiny_dlna;
// We'll feed the XML in two parts to simulate incremental arrival.
const char* xml = R"XML(<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:BrowseResponse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1"><Result>&lt;DIDL-Lite xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dlna="urn:schemas-dlna-org:metadata-1-0/"&gt;
&lt;container id="64" parentID="0" restricted="1" searchable="1" childCount="2"&gt;&lt;dc:title&gt;Browse Folders&lt;/dc:title&gt;&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;&lt;upnp:storageUsed&gt;-1&lt;/upnp:storageUsed&gt;&lt;/container&gt;&lt;container id="1" parentID="0" restricted="1" searchable="1" childCount="7"&gt;&lt;dc:title&gt;Music&lt;/dc:title&gt;&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;&lt;upnp:storageUsed&gt;-1&lt;/upnp:storageUsed&gt;&lt;/container&gt;&lt;container id="3" parentID="0" restricted="1" searchable="1" childCount="5"&gt;&lt;dc:title&gt;Pictures&lt;/dc:title&gt;&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;&lt;upnp:storageUsed&gt;-1&lt;/upnp:storageUsed&gt;&lt;/container&gt;&lt;container id="2" parentID="0" restricted="1" searchable="1" childCount="3"&gt;&lt;dc:title&gt;Video&lt;/dc:title&gt;&lt;upnp:class&gt;object.container.storageFolder&lt;/upnp:class&gt;&lt;upnp:storageUsed&gt;-1&lt;/upnp:storageUsed&gt;&lt;/container&gt;&lt;/DIDL-Lite&gt;</Result>
<NumberReturned>4</NumberReturned>
<TotalMatches>4</TotalMatches>
<UpdateID>1</UpdateID></u:BrowseResponse></s:Body></s:Envelope>
)XML";

void printFragment(Str& node, Vector<Str>& path, Str& text, Str& attrs) {
  Serial.print("Node: ");
  Serial.println(node.c_str());
  // Print path and node
  Serial.print("Path: ");
  for (int i = 0; i < path.size(); ++i) {
    Serial.print(path[i].c_str());
    if (i + 1 < path.size()) Serial.print("/");
  }
  Serial.println();
  if (!text.isEmpty()) {
    Serial.print("Text: ");
    Serial.println(text.c_str());
  }
  if (!attrs.isEmpty()) {
    Serial.print("Attrs: ");
    Serial.println(attrs.c_str());
  }
  Serial.println("--------------------");
}

void setup() {
  Serial.begin(115200);
  Serial.println("XMLParserPrint test starting");

  XMLParserPrint xp;
  xp.setExpandEncoded(true);

  Str node(40);
  Vector<Str> path{5};
  Str text(100);
  Str attrs(100);

  int len = strlen((const char*)xml);
  int open = len;

  while (open > 0) {
    int chunk = open > 200 ? 200 : open;
    xp.write((const uint8_t*)(xml + (len - open)), chunk);
    while (xp.parse(node, path, text, attrs)) {
      printFragment(node, path, text, attrs);
    }
    open -= chunk;
  }

  Serial.println("Done");
}

void loop() {}
