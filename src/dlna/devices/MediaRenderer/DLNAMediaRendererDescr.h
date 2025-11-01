#pragma once

#include "dlna/DLNADescr.h"
#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief AVTransport service descriptor (SCPD) generator
 *
 * This class implements the DLNA AVTransport service description printer.
 * It produces the service SCPD XML by writing into any Arduino-style
 * Print implementation (for example a ChunkPrint or a Null/Measure print).
 * The primary method is printDescr(Print& out) which emits the XML and
 * returns the number of bytes written. The implementation is stateless and
 * safe to construct on-demand where needed.
 */
class DLNAMediaRendererTransportDescr : public DLNADescr {
 public:
  size_t printDescr(Print& out) override {
    using tiny_dlna::XMLPrinter;
    XMLPrinter xml(out);
    size_t result = 0;
    result += xml.printXMLHeader();
    result += xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

    result += xml.printNodeBeginNl("specVersion");
    result += xml.printNode("major", 1);
    result += xml.printNode("minor", 0);
    result += xml.printNodeEnd("specVersion");

    result += xml.printNodeBeginNl("actionList");

    // Pause
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Pause");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // SetRecordQualityMode
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SetRecordQualityMode");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("NewRecordQualityMode", "in", "CurrentRecordQualityMode");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Stop
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Stop");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetPositionInfo
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetPositionInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("Track", "out", "CurrentTrack");
    result += xml.printArgument("TrackDuration", "out", "CurrentTrackDuration");
    result += xml.printArgument("TrackMetaData", "out", "CurrentTrackMetaData");
    result += xml.printArgument("TrackURI", "out", "CurrentTrackURI");
    result += xml.printArgument("RelTime", "out", "RelativeTimePosition");
    result += xml.printArgument("AbsTime", "out", "AbsoluteTimePosition");
    result += xml.printArgument("RelCount", "out", "RelativeCounterPosition");
    result += xml.printArgument("AbsCount", "out", "AbsoluteCounterPosition");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // SetPlayMode
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SetPlayMode");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("NewPlayMode", "in", "CurrentPlayMode");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Play
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Play");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("Speed", "in", "TransportPlaySpeed");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetDeviceCapabilities
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetDeviceCapabilities");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("PlayMedia", "out", "PossiblePlaybackStorageMedia");
    result += xml.printArgument("RecMedia", "out", "PossibleRecordStorageMedia");
    result += xml.printArgument("RecQualityModes", "out", "PossibleRecordQualityModes");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetMediaInfo
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetMediaInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("NrTracks", "out", "NumberOfTracks");
    result += xml.printArgument("MediaDuration", "out", "CurrentMediaDuration");
    result += xml.printArgument("CurrentURI", "out", "AVTransportURI");
    result += xml.printArgument("CurrentURIMetaData", "out", "AVTransportURIMetaData");
    result += xml.printArgument("NextURI", "out", "NextAVTransportURI");
    result += xml.printArgument("NextURIMetaData", "out", "NextAVTransportURIMetaData");
    result += xml.printArgument("PlayMedium", "out", "PlaybackStorageMedium");
    result += xml.printArgument("RecordMedium", "out", "RecordStorageMedium");
    result += xml.printArgument("WriteStatus", "out", "RecordMediumWriteStatus");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Next
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Next");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Previous
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Previous");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetTransportInfo
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetTransportInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("CurrentTransportState", "out", "TransportState");
    result += xml.printArgument("CurrentTransportStatus", "out", "TransportStatus");
    result += xml.printArgument("CurrentSpeed", "out", "TransportPlaySpeed");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Record
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Record");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // SetAVTransportURI
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SetAVTransportURI");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("CurrentURI", "in", "AVTransportURI");
    result += xml.printArgument("CurrentURIMetaData", "in", "AVTransportURIMetaData");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetTransportSettings
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetTransportSettings");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("PlayMode", "out", "CurrentPlayMode");
    result += xml.printArgument("RecQualityMode", "out", "CurrentRecordQualityMode");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // GetCurrentTransportActions
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetCurrentTransportActions");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("Actions", "out", "CurrentTransportActions");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    // Seek
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Seek");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("Unit", "in", "A_ARG_TYPE_SeekMode");
    result += xml.printArgument("Target", "in", "A_ARG_TYPE_SeekTarget");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeEnd("actionList");

    // serviceStateTable
    result += xml.printNodeBeginNl("serviceStateTable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "AbsoluteTimePosition");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentTrackURI");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentTrackMetaData");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "RelativeCounterPosition");
    result += xml.printNode("dataType", "i4");
    result += xml.printNode("defaultValue", 2147483647);
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "AVTransportURIMetaData");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "TransportStatus");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "OK");
    result += xml.printNode("allowedValue", "ERROR_OCCURED");
    result += xml.printNode("allowedValue", "CUSTOM");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "TransportState");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "STOPPED");
    result += xml.printNode("allowedValue", "PLAYING");
    result += xml.printNode("allowedValue", "TRANSITIONING");
    result += xml.printNode("allowedValue", "PAUSED_PLAYBACK");
    result += xml.printNode("allowedValue", "PAUSED_RECORDING");
    result += xml.printNode("allowedValue", "RECORDING");
    result += xml.printNode("allowedValue", "NO_MEDIA_PRESENT");
    result += xml.printNode("allowedValue", "CUSTOM");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentTrack");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNode("defaultValue", 0);
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "NextAVTransportURIMetaData");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    // PlaybackStorageMedium allowed values (long list)
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "PlaybackStorageMedium");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NETWORK");
    result += xml.printNodeBeginNl("allowedValueList");
    const char* playbackValues[] = {"UNKNOWN","DV","MINI-DV","VHS","W-VHS","S-VHS","D-VHS","VHSC","VIDEO8","HI8","CD-ROM","CD-DA","CD-R","CD-RW","VIDEO-CD","SACD","M-AUDIO","MD-PICTURE","DVD-ROM","DVD-VIDEO","DVD-R","DVD+RW","DVD-RW","DVD-RAM","DVD-AUDIO","DAT","LD","HDD","MICRO_MV","NETWORK","NONE","NOT_IMPLEMENTED","VENDOR_SPECIFIC"};
    for (size_t i = 0; i < sizeof(playbackValues)/sizeof(playbackValues[0]); ++i) {
      result += xml.printNode("allowedValue", playbackValues[i]);
    }
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "PossibleRecordQualityModes");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "NumberOfTracks");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNode("defaultValue", 0);
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentMediaDuration");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "00:00:00");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_SeekTarget");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "NextAVTransportURI");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    // RecordStorageMedium allowed values (same as playback)
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "RecordStorageMedium");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeBeginNl("allowedValueList");
    for (size_t i = 0; i < sizeof(playbackValues)/sizeof(playbackValues[0]); ++i) {
      result += xml.printNode("allowedValue", playbackValues[i]);
    }
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "AVTransportURI");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "TransportPlaySpeed");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "1");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "AbsoluteCounterPosition");
    result += xml.printNode("dataType", "i4");
    result += xml.printNode("defaultValue", 2147483647);
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "RelativeTimePosition");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentTrackDuration");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentPlayMode");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NORMAL");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "NORMAL");
    result += xml.printNode("allowedValue", "SHUFFLE");
    result += xml.printNode("allowedValue", "REPEAT_ONE");
    result += xml.printNode("allowedValue", "REPEAT_ALL");
    result += xml.printNode("allowedValue", "RANDOM");
    result += xml.printNode("allowedValue", "DIRECT_1");
    result += xml.printNode("allowedValue", "INTRO");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "PossiblePlaybackStorageMedia");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NETWORK");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_InstanceID");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNodeEnd("stateVariable");

    // CurrentRecordQualityMode
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentRecordQualityMode");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "0:EP");
    result += xml.printNode("allowedValue", "1:LP");
    result += xml.printNode("allowedValue", "2:SP");
    result += xml.printNode("allowedValue", "0:BASIC");
    result += xml.printNode("allowedValue", "1:MEDIUM");
    result += xml.printNode("allowedValue", "2:HIGH");
    result += xml.printNode("allowedValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    // RecordMediumWriteStatus
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "RecordMediumWriteStatus");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "WRITABLE");
    result += xml.printNode("allowedValue", "PROTECTED");
    result += xml.printNode("allowedValue", "NOT_WRITABLE");
    result += xml.printNode("allowedValue", "UNKNOWN");
    result += xml.printNode("allowedValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "CurrentTransportActions");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    // A_ARG_TYPE_SeekMode
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_SeekMode");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "TRACK_NR");
    result += xml.printNode("allowedValue", "ABS_TIME");
    result += xml.printNode("allowedValue", "REL_TIME");
    result += xml.printNode("allowedValue", "ABS_COUNT");
    result += xml.printNode("allowedValue", "REL_COUNT");
    result += xml.printNode("allowedValue", "CHANNEL_FREQ");
    result += xml.printNode("allowedValue", "TAPE-INDEX");
    result += xml.printNode("allowedValue", "FRAME");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "PossibleRecordStorageMedia");
    result += xml.printNode("dataType", "string");
    result += xml.printNode("defaultValue", "NOT_IMPLEMENTED");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "LastChange");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeEnd("serviceStateTable");

    result += xml.printNodeEnd("scpd");
    return result;
  }
};

/**
 * @brief RenderingControl service descriptor (SCPD) generator
 *
 * This class implements the DLNA RenderingControl service description.
 * Use printDescr(Print& out) to emit the SCPD XML to any Print-backed
 * output. The method returns the number of bytes written so callers can
 * measure content-length when streaming HTTP responses without buffering.
 * The class does not keep runtime state and may be constructed per-call.
 */
class DLNAMediaRendererControlDescr : public DLNADescr {
 public:
  size_t printDescr(Print& out) override {
    using tiny_dlna::XMLPrinter;
    XMLPrinter xml(out);
    size_t result = 0;
    result += xml.printXMLHeader();
    result += xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

    result += xml.printNodeBeginNl("specVersion");
    result += xml.printNode("major", 1);
    result += xml.printNode("minor", 0);
    result += xml.printNodeEnd("specVersion");

    // actionList: GetMute, GetVolume, SelectPreset, SetVolume, ListPresets, SetMute
    result += xml.printNodeBeginNl("actionList");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetMute");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("Channel", "in", "A_ARG_TYPE_Channel");
    result += xml.printArgument("CurrentMute", "out", "Mute");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetVolume");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("Channel", "in", "A_ARG_TYPE_Channel");
    result += xml.printArgument("CurrentVolume", "out", "Volume");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SelectPreset");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("PresetName", "in", "A_ARG_TYPE_PresetName");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SetVolume");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_InstanceID");
    result += xml.printArgument("Channel", "in", "A_ARG_TYPE_Channel");
    result += xml.printArgument("DesiredVolume", "in", "Volume");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "ListPresets");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("CurrentPresetNameList", "out", "PresetNameList");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "SetMute");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("InstanceID", "in", "A_ARG_TYPE_INSTANCEID");
    result += xml.printArgument("Channel", "in", "A_ARG_TYPE_Channel");
    result += xml.printArgument("DesiredMute", "in", "Mute");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeEnd("actionList");

    result += xml.printNodeBeginNl("serviceStateTable");

    // A_ARG_TYPE_PresetName
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_PresetName");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "FactoryDefault");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    // Volume with allowedValueRange
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "Volume");
    result += xml.printNode("dataType", "ui2");
    result += xml.printNodeBeginNl("allowedValueRange");
    result += xml.printNode("minimum", 0);
    result += xml.printNode("maximum", 15);
    result += xml.printNode("step", 1);
    result += xml.printNodeEnd("allowedValueRange");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printStateVariable("LastChange", "string", true);

    // A_ARG_TYPE_Channel
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Channel");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "Master");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printStateVariable("PresetNameList", "string", false);
    result += xml.printStateVariable("Mute", "boolean", false);
    result += xml.printStateVariable("A_ARG_TYPE_InstanceID", "ui4", false);

    result += xml.printNodeEnd("serviceStateTable");
    result += xml.printNodeEnd("scpd");
    return result;
  }
};

/**
 * @brief ConnectionManager service descriptor (SCPD) generator
 *
 * Produces the ConnectionManager SCPD XML via printDescr(Print& out).
 * The returned size_t value equals the number of bytes written. The
 * implementation follows the same stateless pattern as the other
 * descriptor classes and is intended to be used when responding to
 * service description requests (service.xml).
 */
class DLNAMediaRendererConnectionMgrDescr : public DLNADescr {
 public:
  size_t printDescr(Print& out) override {
     using tiny_dlna::XMLPrinter;
    XMLPrinter xml(out);
    size_t result = 0;
    result += xml.printXMLHeader();
    result += xml.printNodeBeginNl("scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

    result += xml.printNodeBeginNl("specVersion");
    result += xml.printNode("major", 1);
    result += xml.printNode("minor", 0);
    result += xml.printNodeEnd("specVersion");

    result += xml.printNodeBeginNl("actionList");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetCurrentConnectionIDs");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("ConnectionIDs", "out", "CurrentConnectionIDs");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetProtocolInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("Source", "out", "SourceProtocolInfo");
    result += xml.printArgument("Sink", "out", "SinkProtocolInfo");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetCurrentConnectionInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("ConnectionID", "in", "A_ARG_TYPE_ConnectionID");
    result += xml.printArgument("RcsID", "out", "A_ARG_TYPE_RcsID");
    result += xml.printArgument("AVTransportID", "out", "A_ARG_TYPE_AVTransportID");
    result += xml.printArgument("ProtocolInfo", "out", "A_ARG_TYPE_ProtocolInfo");
    result += xml.printArgument("PeerConnectionManager", "out", "A_ARG_TYPE_ConnectionManager");
    result += xml.printArgument("PeerConnectionID", "out", "A_ARG_TYPE_ConnectionID");
    result += xml.printArgument("Direction", "out", "A_ARG_TYPE_Direction");
    result += xml.printArgument("Status", "out", "A_ARG_TYPE_ConnectionStatus");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeEnd("actionList");

    result += xml.printNodeBeginNl("serviceStateTable");

    result += xml.printStateVariable("SinkProtocolInfo", "string", true);
    result += xml.printStateVariable("A_ARG_TYPE_RcsID", "i4", false);
    result += xml.printStateVariable("CurrentConnectionIDs", "string", true);
    result += xml.printStateVariable("A_ARG_TYPE_ConnectionManager", "string", false);

    // A_ARG_TYPE_ConnectionStatus
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_ConnectionStatus");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "OK");
    result += xml.printNode("allowedValue", "ContentFormatMismatch");
    result += xml.printNode("allowedValue", "InsufficientBandwidth");
    result += xml.printNode("allowedValue", "UnreliableChannel");
    result += xml.printNode("allowedValue", "Unknown");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    // A_ARG_TYPE_Direction
    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Direction");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "Output");
    result += xml.printNode("allowedValue", "Input");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printStateVariable("A_ARG_TYPE_ProtocolInfo", "string", false);
    result += xml.printStateVariable("SourceProtocolInfo", "string", true);
    result += xml.printStateVariable("A_ARG_TYPE_ConnectionID", "i4", false);
    result += xml.printStateVariable("A_ARG_TYPE_AVTransportID", "i4", false);

    result += xml.printNodeEnd("serviceStateTable");
    result += xml.printNodeEnd("scpd");
    return result;
  }
};

}  // namespace tiny_dlna
