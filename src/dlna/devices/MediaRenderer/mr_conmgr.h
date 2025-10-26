#pragma once

/**
"<?xml version="1.0" encoding="utf-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
  <specVersion>
    <major>1</major>
    <minor>0</minor>
  </specVersion>
  <actionList>
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
  </actionList>
  <serviceStateTable>
    <stateVariable sendEvents="no">
      <name>SourceProtocolInfo</name>
      <dataType>string</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>SinkProtocolInfo</name>
      <dataType>string</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>CurrentConnectionIDs</name>
      <dataType>string</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
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
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_ConnectionManager</name>
      <dataType>string</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_Direction</name>
      <dataType>string</dataType>
      <allowedValueList>
        <allowedValue>Input</allowedValue>
        <allowedValue>Output</allowedValue>
      </allowedValueList>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_ProtocolInfo</name>
      <dataType>string</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_ConnectionID</name>
      <dataType>i4</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_AVTransportID</name>
      <dataType>i4</dataType>
    </stateVariable>
    <stateVariable sendEvents="no">
      <name>A_ARG_TYPE_RcsID</name>
      <dataType>i4</dataType>
    </stateVariable>
  </serviceStateTable>
</scpd>";
*/

#include "dlna/xml/XMLPrinter.h"

static void mr_connmgr_xml_printer(Print& out) {
  using tiny_dlna::XMLPrinter;
  XMLPrinter xml(out);
  xml.printXMLHeader();
  xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

  xml.printNodeBeginNl("specVersion");
  xml.printNode("major", 1);
  xml.printNode("minor", 0);
  xml.printNodeEnd("specVersion");

  // actionList
  xml.printNodeBeginNl("actionList");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetProtocolInfo");
  xml.printNodeBeginNl("argumentList");
  xml.printArgument("Source", "out", "SourceProtocolInfo");
  xml.printArgument("Sink", "out", "SinkProtocolInfo");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

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
  xml.printArgument("ConnectionID", "in", "A_ARG_TYPE_ConnectionID");
  xml.printArgument("RcsID", "out", "A_ARG_TYPE_RcsID");
  xml.printArgument("AVTransportID", "out", "A_ARG_TYPE_AVTransportID");
  xml.printArgument("ProtocolInfo", "out", "A_ARG_TYPE_ProtocolInfo");
  xml.printArgument("PeerConnectionManager", "out",
                    "A_ARG_TYPE_ConnectionManager");
  xml.printArgument("PeerConnectionID", "out", "A_ARG_TYPE_ConnectionID");
  xml.printArgument("Direction", "out", "A_ARG_TYPE_Direction");
  xml.printArgument("Status", "out", "A_ARG_TYPE_ConnectionStatus");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  // serviceStateTable
  xml.printNodeBeginNl("serviceStateTable");

  xml.printStateVariable("SourceProtocolInfo", "string", false);
  xml.printStateVariable("SinkProtocolInfo", "string", false);
  xml.printStateVariable("CurrentConnectionIDs", "string", false);
  xml.printStateVariable("A_ARG_TYPE_ConnectionStatus", "string", false, [&]() {
    xml.printNodeBeginNl("allowedValueList");
    xml.printNode("allowedValue", "OK");
    xml.printNode("allowedValue", "ContentFormatMismatch");
    xml.printNode("allowedValue", "InsufficientBandwidth");
    xml.printNode("allowedValue", "UnreliableChannel");
    xml.printNode("allowedValue", "Unknown");
    xml.printNodeEnd("allowedValueList");
  });
  xml.printStateVariable("A_ARG_TYPE_ConnectionManager", "string", false);
  xml.printStateVariable("A_ARG_TYPE_Direction", "string", false, [&]() {
    xml.printNodeBeginNl("allowedValueList");
    xml.printNode("allowedValue", "Input");
    xml.printNode("allowedValue", "Output");
    xml.printNodeEnd("allowedValueList");
  });
  xml.printStateVariable("A_ARG_TYPE_ProtocolInfo", "string", false);
  xml.printStateVariable("A_ARG_TYPE_ConnectionID", "i4", false);
  xml.printStateVariable("A_ARG_TYPE_AVTransportID", "i4", false);
  xml.printStateVariable("A_ARG_TYPE_RcsID", "i4", false);

  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
