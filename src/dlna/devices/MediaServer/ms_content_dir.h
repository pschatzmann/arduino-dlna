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
  xml.printArgument("ObjectID", "in", "A_ARG_TYPE_ObjectID");
  xml.printArgument("BrowseFlag", "in", "A_ARG_TYPE_BrowseFlag");
  xml.printArgument("Filter", "in", "A_ARG_TYPE_Filter");
  xml.printArgument("StartingIndex", "in", "A_ARG_TYPE_Index");
  xml.printArgument("RequestedCount", "in", "A_ARG_TYPE_Count");
  xml.printArgument("SortCriteria", "in", "A_ARG_TYPE_SortCriteria");
  xml.printArgument("Result", "out", "A_ARG_TYPE_Result");
  xml.printArgument("NumberReturned", "out", "A_ARG_TYPE_Count");
  xml.printArgument("TotalMatches", "out", "A_ARG_TYPE_Count");
  xml.printArgument("UpdateID", "out", "A_ARG_TYPE_UpdateID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSearchCapabilities");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("SearchCaps", "out", "SearchCapabilities");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSortCapabilities");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("SortCaps", "out", "SortCapabilities");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetSystemUpdateID");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("Id", "out", "SystemUpdateID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  /* Search action */
  xml.printNodeBeginNl("action");
  xml.printNode("name", "Search");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("ContainerID", "in", "A_ARG_TYPE_ObjectID");
  xml.printArgument("SearchCriteria", "in", "A_ARG_TYPE_SearchCriteria");
  xml.printArgument("Filter", "in", "A_ARG_TYPE_Filter");
  xml.printArgument("StartingIndex", "in", "A_ARG_TYPE_Index");
  xml.printArgument("RequestedCount", "in", "A_ARG_TYPE_Count");
  xml.printArgument("SortCriteria", "in", "A_ARG_TYPE_SortCriteria");
  xml.printArgument("Result", "out", "A_ARG_TYPE_Result");
  xml.printArgument("NumberReturned", "out", "A_ARG_TYPE_Count");
  xml.printArgument("TotalMatches", "out", "A_ARG_TYPE_Count");
  xml.printArgument("UpdateID", "out", "A_ARG_TYPE_UpdateID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  /* UpdateObject action */
  xml.printNodeBeginNl("action");
  xml.printNode("name", "UpdateObject");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("ObjectID", "in", "A_ARG_TYPE_ObjectID");
  xml.printArgument("CurrentTagValue", "in", "A_ARG_TYPE_TagValueList");
  xml.printArgument("NewTagValue", "in", "A_ARG_TYPE_TagValueList");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  xml.printNodeBeginNl("serviceStateTable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "TransferIDs");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_ObjectID");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Result");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_SearchCriteria");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_BrowseFlag");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "BrowseMetadata");
  xml.printNode("allowedValue", "BrowseDirectChildren");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Filter");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_SortCriteria");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Index");
  xml.printNode("dataType", "i4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Count");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_UpdateID");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_TagValueList");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "SearchCapabilities");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "SortCapabilities");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "SystemUpdateID");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
