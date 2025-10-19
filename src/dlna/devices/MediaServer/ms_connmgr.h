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
<name>GetCurrentConnectionIDs</name>
<argumentList>
<argument>
<name>ConnectionIDs</name>
<direction>out</direction>
<relatedStateVariable>CurrentConnectionIDs</relatedStateVariable>
</argument>
</argumentList>
</action>
<action>
<name>GetCurrentConnectionInfo</name>
<argumentList>
<argument>
<name>ConnectionID</name>
<direction>in</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
</argument>
<argument>
<name>RcsID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_RcsID</relatedStateVariable>
</argument>
<argument>
<name>AVTransportID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_AVTransportID</relatedStateVariable>
</argument>
<argument>
<name>ProtocolInfo</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_ProtocolInfo</relatedStateVariable>
</argument>
<argument>
<name>PeerConnectionManager</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionManager</relatedStateVariable>
</argument>
<argument>
<name>PeerConnectionID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
</argument>
<argument>
<name>Direction</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_Direction</relatedStateVariable>
</argument>
<argument>
<name>Status</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionStatus</relatedStateVariable>
</argument>
</argumentList>
</action>
<action>
<name>GetProtocolInfo</name>
<argumentList>
<argument>
<name>Source</name>
<direction>out</direction>
<relatedStateVariable>SourceProtocolInfo</relatedStateVariable>
</argument>
<argument>
<name>Sink</name>
<direction>out</direction>
<relatedStateVariable>SinkProtocolInfo</relatedStateVariable>
</argument>
</argumentList>
</action>
<action>
<name>PrepareForConnection</name>
<argumentList>
<argument>
<name>RemoteProtocolInfo</name>
<direction>in</direction>
<relatedStateVariable>A_ARG_TYPE_ProtocolInfo</relatedStateVariable>
</argument>
<argument>
<name>PeerConnectionManager</name>
<direction>in</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionManager</relatedStateVariable>
</argument>
<argument>
<name>PeerConnectionID</name>
<direction>in</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
</argument>
<argument>
<name>Direction</name>
<direction>in</direction>
<relatedStateVariable>A_ARG_TYPE_Direction</relatedStateVariable>
</argument>
<argument>
<name>ConnectionID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
</argument>
<argument>
<name>AVTransportID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_AVTransportID</relatedStateVariable>
</argument>
<argument>
<name>RcsID</name>
<direction>out</direction>
<relatedStateVariable>A_ARG_TYPE_RcsID</relatedStateVariable>
</argument>
</argumentList>
</action>
</actionList>
<serviceStateTable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_ConnectionManager</name>
<dataType>string</dataType>
</stateVariable>
<stateVariable sendEvents=\"yes\">
<name>SinkProtocolInfo</name>
<dataType>string</dataType>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_ConnectionStatus</name>
<dataType>string</dataType>
<allowedValueList>
<allowedValue>OK</allowedValue>
<allowedValue>ContentFormatMismatch</allowedValue>
<allowedValue>InsufficientBandwidth</allowedValue>
<allowedValue>UnreliableChannel</allowedValue>
<allowedValue>Unknown</allowedValue>
</allowedValueList>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_AVTransportID</name>
<dataType>i4</dataType>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_Direction</name>
<dataType>string</dataType>
<allowedValueList>
<allowedValue>Input</allowedValue>
<allowedValue>Output</allowedValue>
</allowedValueList>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_RcsID</name>
<dataType>i4</dataType>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_ProtocolInfo</name>
<dataType>string</dataType>
</stateVariable>
<stateVariable sendEvents=\"no\">
<name>A_ARG_TYPE_ConnectionID</name>
<dataType>i4</dataType>
</stateVariable>
<stateVariable sendEvents=\"yes\">
<name>SourceProtocolInfo</name>
<dataType>string</dataType>
</stateVariable>
<stateVariable sendEvents=\"yes\">
<name>CurrentConnectionIDs</name>
<dataType>string</dataType>
</stateVariable>
</serviceStateTable>
</scpd>\n";
*/

#include "dlna/xml/XMLPrinter.h"

static void ms_connmgr_xml_printer(Print& out) {
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
  xml.printNode("name", "GetCurrentConnectionIDs");
  xml.printNodeBeginNl("argumentList");
  xml.printNodeBeginNl("argument");
  xml.printNode("name", "ConnectionIDs");
  xml.printNode("direction", "out");
  xml.printNode("relatedStateVariable", "CurrentConnectionIDs");
  xml.printNodeEnd("argument");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetCurrentConnectionInfo");
  xml.printNodeBeginNl("argumentList");
  auto argInOut = [&](const char* n, const char* dir, const char* rel) {
    xml.printNodeBeginNl("argument");
    xml.printNode("name", n);
    xml.printNode("direction", dir);
    xml.printNode("relatedStateVariable", rel);
    xml.printNodeEnd("argument");
  };
  argInOut("ConnectionID", "in", "A_ARG_TYPE_ConnectionID");
  argInOut("RcsID", "out", "A_ARG_TYPE_RcsID");
  argInOut("AVTransportID", "out", "A_ARG_TYPE_AVTransportID");
  argInOut("ProtocolInfo", "out", "A_ARG_TYPE_ProtocolInfo");
  argInOut("PeerConnectionManager", "out", "A_ARG_TYPE_ConnectionManager");
  argInOut("PeerConnectionID", "out", "A_ARG_TYPE_ConnectionID");
  argInOut("Direction", "out", "A_ARG_TYPE_Direction");
  argInOut("Status", "out", "A_ARG_TYPE_ConnectionStatus");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetProtocolInfo");
  xml.printNodeBeginNl("argumentList");
  argInOut("Source", "out", "SourceProtocolInfo");
  argInOut("Sink", "out", "SinkProtocolInfo");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "PrepareForConnection");
  xml.printNodeBeginNl("argumentList");
  argInOut("RemoteProtocolInfo", "in", "A_ARG_TYPE_ProtocolInfo");
  argInOut("PeerConnectionManager", "in", "A_ARG_TYPE_ConnectionManager");
  argInOut("PeerConnectionID", "in", "A_ARG_TYPE_ConnectionID");
  argInOut("Direction", "in", "A_ARG_TYPE_Direction");
  argInOut("ConnectionID", "out", "A_ARG_TYPE_ConnectionID");
  argInOut("AVTransportID", "out", "A_ARG_TYPE_AVTransportID");
  argInOut("RcsID", "out", "A_ARG_TYPE_RcsID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  xml.printNodeBeginNl("serviceStateTable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_ConnectionManager");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "SinkProtocolInfo");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_ConnectionStatus");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "OK");
  xml.printNode("allowedValue", "ContentFormatMismatch");
  xml.printNode("allowedValue", "InsufficientBandwidth");
  xml.printNode("allowedValue", "UnreliableChannel");
  xml.printNode("allowedValue", "Unknown");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_AVTransportID");
  xml.printNode("dataType", "i4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Direction");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "Input");
  xml.printNode("allowedValue", "Output");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_RcsID");
  xml.printNode("dataType", "i4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_ProtocolInfo");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_ConnectionID");
  xml.printNode("dataType", "i4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "SourceProtocolInfo");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "CurrentConnectionIDs");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
