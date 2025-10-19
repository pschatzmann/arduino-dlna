#pragma once
/**
 *<?xml version=\*1.0\* encoding=\*utf-8\*?>
 *<scpd xmlns=\*urn:schemas-upnp-org:service-1-0\*>
 *  <specVersion>
 *    <major>1</major>
 *    <minor>0</minor>
 *  </specVersion>
 *  <actionList>
 *    <action>
 *      <name>GetVolume</name>
 *      <argumentList>
 *        <argument>
 *          <name>InstanceID</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>Channel</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>CurrentVolume</name>
 *          <direction>out</direction>
 *          <relatedStateVariable>Volume</relatedStateVariable>
 *        </argument>
 *      </argumentList>
 *    </action>
 *    <action>
 *      <name>SetVolume</name>
 *      <argumentList>
 *        <argument>
 *          <name>InstanceID</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>Channel</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>DesiredVolume</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>Volume</relatedStateVariable>
 *        </argument>
 *      </argumentList>
 *    </action>
 *    <action>
 *      <name>GetMute</name>
 *      <argumentList>
 *        <argument>
 *          <name>InstanceID</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>Channel</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>CurrentMute</name>
 *          <direction>out</direction>
 *          <relatedStateVariable>Mute</relatedStateVariable>
 *        </argument>
 *      </argumentList>
 *    </action>
 *    <action>
 *      <name>SetMute</name>
 *      <argumentList>
 *        <argument>
 *          <name>InstanceID</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>Channel</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>A_ARG_TYPE_Channel</relatedStateVariable>
 *        </argument>
 *        <argument>
 *          <name>DesiredMute</name>
 *          <direction>in</direction>
 *          <relatedStateVariable>Mute</relatedStateVariable>
 *        </argument>
 *      </argumentList>
 *    </action>
 *  </actionList>
 *  <serviceStateTable>
 *    <stateVariable sendEvents=\*no\*>
 *      <name>Volume</name>
 *      <dataType>ui2</dataType>
 *      <allowedValueRange>
 *        <minimum>0</minimum>
 *        <maximum>100</maximum>
 *        <step>1</step>
 *      </allowedValueRange>
 *    </stateVariable>
 *    <stateVariable sendEvents=\*no\*>
 *      <name>Mute</name>
 *      <dataType>boolean</dataType>
 *    </stateVariable>
 *    <stateVariable sendEvents=\*no\*>
 *      <name>A_ARG_TYPE_InstanceID</name>
 *      <dataType>ui4</dataType>
 *    </stateVariable>
 *    <stateVariable sendEvents=\*no\*>
 *      <name>A_ARG_TYPE_Channel</name>
 *      <dataType>string</dataType>
 *      <allowedValueList>
 *        <allowedValue>Master</allowedValue>
 *      </allowedValueList>
 *    </stateVariable>
 *  </serviceStateTable>
 */

#include "dlna/xml/XMLPrinter.h"

/// Streaming generator for RenderingControl SCPD
static void mr_control_xml_printer(Print& out) {
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
  xml.printNode("name", "GetVolume");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("Channel", "in", "A_ARG_TYPE_Channel");
  arg("CurrentVolume", "out", "Volume");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "SetVolume");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("Channel", "in", "A_ARG_TYPE_Channel");
  arg("DesiredVolume", "in", "Volume");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "GetMute");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("Channel", "in", "A_ARG_TYPE_Channel");
  arg("CurrentMute", "out", "Mute");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeBeginNl("action");
  xml.printNode("name", "SetMute");
  xml.printNodeBeginNl("argumentList");
  arg("InstanceID", "in", "A_ARG_TYPE_InstanceID");
  arg("Channel", "in", "A_ARG_TYPE_Channel");
  arg("DesiredMute", "in", "Mute");
  xml.printNodeEnd("argumentList");
  xml.printNodeEnd("action");

  xml.printNodeEnd("actionList");

  xml.printNodeBeginNl("serviceStateTable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "Volume");
  xml.printNode("dataType", "ui2");
  xml.printNodeBeginNl("allowedValueRange");
  xml.printNode("minimum", 0);
  xml.printNode("maximum", 100);
  xml.printNode("step", 1);
  xml.printNodeEnd("allowedValueRange");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "Mute");
  xml.printNode("dataType", "boolean");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_InstanceID");
  xml.printNode("dataType", "ui4");
  xml.printNodeEnd("stateVariable");

  xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
  xml.printNode("name", "A_ARG_TYPE_Channel");
  xml.printNode("dataType", "string");
  xml.printNodeBeginNl("allowedValueList");
  xml.printNode("allowedValue", "Master");
  xml.printNodeEnd("allowedValueList");
  xml.printNodeEnd("stateVariable");

  xml.printNodeEnd("serviceStateTable");

  xml.printNodeEnd("scpd");
}
