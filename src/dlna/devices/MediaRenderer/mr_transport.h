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
        <name>SetAVTransportURI</name>
        <argumentList>
          <argument>
            <name>InstanceID</name>
            <direction>in</direction>
            <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
          </argument>
          <argument>
            <name>CurrentURI</name>
            <direction>in</direction>
            <relatedStateVariable>AVTransportURI</relatedStateVariable>
          </argument>
          <argument>
            <name>CurrentURIMetaData</name>
            <direction>in</direction>
            <relatedStateVariable>AVTransportURIMetaData</relatedStateVariable>
          </argument>
        </argumentList>
      </action>
      <action>
        <name>Play</name>
        <argumentList>
          <argument>
            <name>InstanceID</name>
            <direction>in</direction>
            <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
          </argument>
          <argument>
            <name>Speed</name>
            <direction>in</direction>
            <relatedStateVariable>TransportPlaySpeed</relatedStateVariable>
          </argument>
        </argumentList>
      </action>
      <action>
        <name>Pause</name>
        <argumentList>
          <argument>
            <name>InstanceID</name>
            <direction>in</direction>
            <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
          </argument>
        </argumentList>
      </action>
      <action>
        <name>Stop</name>
        <argumentList>
          <argument>
            <name>InstanceID</name>
            <direction>in</direction>
            <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
          </argument>
        </argumentList>
      </action>
      <action>
        <name>GetTransportInfo</name>
        <argumentList>
          <argument>
            <name>InstanceID</name>
            <direction>in</direction>
            <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
          </argument>
          <argument>
            <name>CurrentTransportState</name>
            <direction>out</direction>
            <relatedStateVariable>TransportState</relatedStateVariable>
          </argument>
          <argument>
            <name>CurrentTransportStatus</name>
            <direction>out</direction>
            <relatedStateVariable>TransportStatus</relatedStateVariable>
          </argument>
          <argument>
            <name>CurrentSpeed</name>
            <direction>out</direction>
            <relatedStateVariable>TransportPlaySpeed</relatedStateVariable>
          </argument>
        </argumentList>
      </action>
    </actionList>
    <serviceStateTable>
      <stateVariable sendEvents=\"no\">
        <name>AVTransportURI</name>
        <dataType>string</dataType>
      </stateVariable>
      <stateVariable sendEvents=\"no\">
        <name>AVTransportURIMetaData</name>
        <dataType>string</dataType>
      </stateVariable>
      <stateVariable sendEvents=\"no\">
        <name>TransportPlaySpeed</name>
        <dataType>string</dataType>
        <allowedValueList>
          <allowedValue>1</allowedValue>
        </allowedValueList>
      </stateVariable>
      <stateVariable sendEvents=\"yes\">
        <name>TransportState</name>
        <dataType>string</dataType>
        <allowedValueList>
          <allowedValue>STOPPED</allowedValue>
          <allowedValue>PAUSED_PLAYBACK</allowedValue>
          <allowedValue>PLAYING</allowedValue>
          <allowedValue>TRANSITIONING</allowedValue>
          <allowedValue>NO_MEDIA_PRESENT</allowedValue>
        </allowedValueList>
      </stateVariable>
      <stateVariable sendEvents=\"no\">
        <name>TransportStatus</name>
        <dataType>string</dataType>
        <allowedValueList>
          <allowedValue>OK</allowedValue>
          <allowedValue>ERROR_OCCURRED</allowedValue>
        </allowedValueList>
      </stateVariable>
      <stateVariable sendEvents=\"no\">
        <name>A_ARG_TYPE_InstanceID</name>
        <dataType>ui4</dataType>
      </stateVariable>
    </serviceStateTable>
  </scpd>";
*/

#include "dlna/xml/XMLPrinter.h"

static void mr_transport_xml_printer(Print& out) {
  using tiny_dlna::XMLPrinter;
  XMLPrinter xml(out);
  xml.printXMLHeader();
  xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

  xml.printNodeBeginNl("specVersion");
  xml.printNode("major", 1);
  xml.printNode("minor", 0);
  xml.printNodeEnd("specVersion");

  xml.printNodeBeginNl("actionList");

  auto arg = [&](const char* n, const char* dir, const char* rel) {
    xml.printNodeBeginNl("argument");
    xml.printNode("name", n);
    xml.printNode("direction", dir);
    xml.printNode("relatedStateVariable", rel);
    xml.printNodeEnd("argument");
  };

  xml.printNodeBeginNl("action");
  xml.printNode("name", "SetAVTransportURI");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("CurrentURI", "in", "AVTransportURI");
  arg("CurrentURIMetaData", "in", "AVTransportURIMetaData");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "Play");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("Speed", "in", "TransportPlaySpeed");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "Pause");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "Stop");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetTransportInfo");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("CurrentTransportState", "out", "TransportState");
  arg("CurrentTransportStatus", "out", "TransportStatus");
  arg("CurrentSpeed", "out", "TransportPlaySpeed");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  xml.printNodeBeginNl("serviceStateTable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "AVTransportURI");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "AVTransportURIMetaData");
  xml.printNode("dataType", "string");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "TransportPlaySpeed");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "1");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
  xml.printNode("name", "TransportState");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "STOPPED");
  xml.printNode("allowedValue", "PAUSED_PLAYBACK");
  xml.printNode("allowedValue", "PLAYING");
  xml.printNode("allowedValue", "TRANSITIONING");
  xml.printNode("allowedValue", "NO_MEDIA_PRESENT");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "TransportStatus");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "OK");
  xml.printNode("allowedValue", "ERROR_OCCURRED");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_InstanceID");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
