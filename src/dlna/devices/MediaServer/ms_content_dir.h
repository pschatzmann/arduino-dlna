#pragma once

/*
<?xml version=\"1.0\"?>
<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">
<specVersion>
<major>1</major>
<minor>0</minor>
</specVersion>
<actionList>
<action>
<name>Browse</name>
<argumentList>
<argument>
<name>ObjectID</name>
<direction>in</direction>
</argument>
<argument>
<name>BrowseFlag</name>
<direction>in</direction>
</argument>
<argument>
<name>Filter</name>
<direction>in</direction>
</argument>
<argument>
<name>StartingIndex</name>
<direction>in</direction>
</argument>
<argument>
<name>RequestedCount</name>
<direction>in</direction>
</argument>
<argument>
<name>SortCriteria</name>
<direction>in</direction>
</argument>
<argument>
<name>Result</name>
<direction>out</direction>
</argument>
<argument>
<name>NumberReturned</name>
<direction>out</direction>
</argument>
<argument>
<name>TotalMatches</name>
<direction>out</direction>
</argument>
<argument>
<name>UpdateID</name>
<direction>out</direction>
</argument>
</argumentList>
</action>
<action>
<name>GetSearchCapabilities</name>
</action>
<action>
<name>GetSortCapabilities</name>
</action>
<action>
<name>GetSystemUpdateID</name>
</action>
</actionList>
<serviceStateTable>
<stateVariable sendEvents=\"no\">
<name>SystemUpdateID</name>
<dataType>ui4</dataType>
</stateVariable>
</serviceStateTable>
</scpd>\n";
*/

#include "dlna/xml/XMLPrinter.h"

static void ms_content_dir_xml_printer(Print& out) {
  using tiny_dlna::XMLPrinter;
  XMLPrinter xml(out);
  xml.printXMLHeader();
  xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

  xml.printNodeBeginNl("specVersion");
  xml.printNode("major", 1);
  xml.printNode("minor", 0);
  xml.printNodeEnd("specVersion");

  xml.printNodeBeginNl("actionList");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "Browse");
  xml.printNodeBeginNl("argumentList");
  auto arg = [&](const char* n, const char* dir) {
    xml.printNodeBeginNl("argument");
    xml.printNode("name", n);
    xml.printNode("direction", dir);
    xml.printNodeEnd("argument");
  };
  arg("ObjectID", "in");
  arg("BrowseFlag", "in");
  arg("Filter", "in");
  arg("StartingIndex", "in");
  arg("RequestedCount", "in");
  arg("SortCriteria", "in");
  arg("Result", "out");
  arg("NumberReturned", "out");
  arg("TotalMatches", "out");
  arg("UpdateID", "out");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSearchCapabilities");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSortCapabilities");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSystemUpdateID");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  xml.printNodeBeginNl("serviceStateTable");
  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "SystemUpdateID");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");
  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
