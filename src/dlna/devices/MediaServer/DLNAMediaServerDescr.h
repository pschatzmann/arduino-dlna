#pragma once

#include "dlna/xml/XMLPrinter.h"

namespace tiny_dlna {

/**
 * @brief ConnectionManager SCPD descriptor for MediaServer
 *
 * DLNAMediaServerConnectionMgrDescr generates the Service Control
 * Protocol Description (SCPD) XML for the ConnectionManager service and
 * writes it to any Arduino-style `Print` instance. The primary method
 * is `printDescr(Print& out)` which emits XML and returns the number of
 * bytes written. That return value can be used to compute HTTP
 * Content-Length before streaming the body.
 *
 * The class is stateless and intended to be constructed on-demand by
 * request handlers.
 */
class DLNAMediaServerConnectionMgrDescr {
 public:
    /**
     * @brief Emit ConnectionManager SCPD XML
     * @param out Print target to write XML to
     * @return number of bytes written
     *
     * Use the returned size to set Content-Length when streaming the
     * response without creating a temporary buffer.
     */
    size_t printDescr(Print& out) {
    using tiny_dlna::XMLPrinter;
    XMLPrinter xml(out);
    size_t result = 0;
    result += xml.printXMLHeader();
    result += xml.printNodeBeginNl(
        "scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

    result += xml.printNodeBeginNl("specVersion");
    result += xml.printNode("major", 1);
    result += xml.printNode("minor", 0);
    result += xml.printNodeEnd("specVersion");

    result += xml.printNodeBeginNl("actionList");
    /* GetProtocolInfo */
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetProtocolInfo");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("Source", "out", "SourceProtocolInfo");
    result += xml.printArgument("Sink", "out", "SinkProtocolInfo");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    /* GetCurrentConnectionIDs */
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetCurrentConnectionIDs");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("ConnectionIDs", "out", "CurrentConnectionIDs");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    /* GetCurrentConnectionInfo */
    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetCurrentConnectionInfo");
    result += xml.printNodeBeginNl("argumentList");
    result +=
        xml.printArgument("ConnectionID", "in", "A_ARG_TYPE_ConnectionID");
    result += xml.printArgument("RcsID", "out", "A_ARG_TYPE_RcsID");
    result +=
        xml.printArgument("AVTransportID", "out", "A_ARG_TYPE_AVTransportID");
    result +=
        xml.printArgument("ProtocolInfo", "out", "A_ARG_TYPE_ProtocolInfo");
    result += xml.printArgument("PeerConnectionManager", "out",
                                "A_ARG_TYPE_ConnectionManager");
    result +=
        xml.printArgument("PeerConnectionID", "out", "A_ARG_TYPE_ConnectionID");
    result += xml.printArgument("Direction", "out", "A_ARG_TYPE_Direction");
    result += xml.printArgument("Status", "out", "A_ARG_TYPE_ConnectionStatus");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");
    result += xml.printNodeEnd("actionList");
    result += xml.printNodeBeginNl("serviceStateTable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "SourceProtocolInfo");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "SinkProtocolInfo");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "CurrentConnectionIDs");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

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

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_ConnectionManager");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Direction");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "Input");
    result += xml.printNode("allowedValue", "Output");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_ProtocolInfo");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_ConnectionID");
    result += xml.printNode("dataType", "i4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_AVTransportID");
    result += xml.printNode("dataType", "i4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_RcsID");
    result += xml.printNode("dataType", "i4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeEnd("serviceStateTable");

    result += xml.printNodeEnd("scpd");
    return result;
  }
};

/**
 * @brief ContentDirectory SCPD descriptor for MediaServer
 *
 * DLNAMediaServerContentDirectoryDescr generates the ContentDirectory
 * service SCPD XML. Use `printDescr(Print& out)` to write the XML to any
 * Print-backed output. The method returns the number of bytes written,
 * enabling callers to measure output size for streaming HTTP responses.
 *
 * The class does not store runtime state and is safe to create per-call.
 */
class DLNAMediaServerContentDirectoryDescr {
 public:
    /**
     * @brief Emit ContentDirectory SCPD XML
     * @param out Print target to write XML to
     * @return number of bytes written
     *
     * The returned byte count can be used to compute a Content-Length
     * header before streaming the response.
     */
    size_t printDescr(Print& out) {
    using tiny_dlna::XMLPrinter;
    XMLPrinter xml(out);
    size_t result = 0;
    result += xml.printXMLHeader();
    result += xml.printNodeBeginNl(
        "scpd", "xmlns=\"urn:schemas-upnp-org:service-1-0\"");

    result += xml.printNodeBeginNl("specVersion");
    result += xml.printNode("major", 1);
    result += xml.printNode("minor", 0);
    result += xml.printNodeEnd("specVersion");

    result += xml.printNodeBeginNl("actionList");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "Browse");
    result += xml.printNodeBeginNl("argumentList");
    result += xml.printArgument("ObjectID", "in", "A_ARG_TYPE_ObjectID");
    result += xml.printArgument("BrowseFlag", "in", "A_ARG_TYPE_BrowseFlag");
    result += xml.printArgument("Filter", "in", "A_ARG_TYPE_Filter");
    result += xml.printArgument("StartingIndex", "in", "A_ARG_TYPE_Index");
    result += xml.printArgument("RequestedCount", "in", "A_ARG_TYPE_Count");
    result +=
        xml.printArgument("SortCriteria", "in", "A_ARG_TYPE_SortCriteria");
    result += xml.printArgument("Result", "out", "A_ARG_TYPE_Result");
    result += xml.printArgument("NumberReturned", "out", "A_ARG_TYPE_Count");
    result += xml.printArgument("TotalMatches", "out", "A_ARG_TYPE_Count");
    result += xml.printArgument("UpdateID", "out", "A_ARG_TYPE_UpdateID");
    result += xml.printNodeEnd("argumentList");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetSearchCapabilities");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetSortCapabilities");
    result += xml.printNodeEnd("action");

    result += xml.printNodeBeginNl("action");
    result += xml.printNode("name", "GetSystemUpdateID");
    result += xml.printNodeEnd("action");

    result += xml.printNodeEnd("actionList");

    result += xml.printNodeBeginNl("serviceStateTable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "TransferIDs");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_ObjectID");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Result");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_SearchCriteria");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_BrowseFlag");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeBeginNl("allowedValueList");
    result += xml.printNode("allowedValue", "BrowseMetadata");
    result += xml.printNode("allowedValue", "BrowseDirectChildren");
    result += xml.printNodeEnd("allowedValueList");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Filter");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_SortCriteria");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Index");
    result += xml.printNode("dataType", "i4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_Count");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_UpdateID");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "A_ARG_TYPE_TagValueList");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "SearchCapabilities");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"no\"");
    result += xml.printNode("name", "SortCapabilities");
    result += xml.printNode("dataType", "string");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeBeginNl("stateVariable", "sendEvents=\"yes\"");
    result += xml.printNode("name", "SystemUpdateID");
    result += xml.printNode("dataType", "ui4");
    result += xml.printNodeEnd("stateVariable");

    result += xml.printNodeEnd("serviceStateTable");

    result += xml.printNodeEnd("scpd");
    return result;
  }
};

}  // namespace tiny_dlna
