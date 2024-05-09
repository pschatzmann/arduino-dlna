/*
 * UPnP.h - Library for creating UPnP rules automatically in your router.
 * Created by Ofek Pearl, September 2017.
 */

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "UPnP.h"

namespace tiny_dlna {

String UpnpRule::toString() {
  char msg[160] = {0};
  sprintf(msg, "%5d %-30s %-18s %7d %7d %7s %7d", this->index,
          this->devFriendlyName.c_str(), toStr(this->internalAddr),
          this->internalPort, this->externalPort, this->protocol.c_str(),
          this->leaseDurationSec);
  return String(msg);
}

void UPnP::GatewayDlnaInfo::clear() {
  host = IPAddress(0, 0, 0, 0);
  port = 0;
  path = "";
  actionPort = 0;
  actionPath = "";
  serviceTypeName = "";
}

bool UPnP::GatewayDlnaInfo::isValid() {
  if (host == NullIP || port == 0 || path.length() == 0 || actionPort == 0) {
    DlnaLogger.log(DlnaError, "Gateway info is not valid: %s", toStr(host));
    return false;
  }
  DlnaLogger.log(
      DlnaInfo,
      "GatewayDlnaInfo::isValid [%s] port [%d] path [%s] actionPort [%d] "
      "actionPath [%s] serviceTypeName [%s]",
      toStr(host), port, path, actionPort, actionPath, serviceTypeName);

  DlnaLogger.log(DlnaInfo, "Gateway info is valid");
  return true;
}

void UPnP::addConfig(IPAddress ruleIP, int ruleInternalPort,
                     int ruleExternalPort, const char *ruleProtocol,
                     int ruleLeaseDurationSec, const char *ruleFriendlyName) {
  static int index = 0;
  UpnpRule rule;
  rule.index = index++;
  rule.internalAddr = (ruleIP == localIP)
                          ? NullIP
                          : ruleIP;  // for automatic IP change handling
  rule.internalPort = ruleInternalPort;
  rule.externalPort = ruleExternalPort;
  rule.leaseDurationSec = ruleLeaseDurationSec;
  rule.protocol = ruleProtocol;
  rule.devFriendlyName = ruleFriendlyName;

  ruleNodes.push_back(rule);
}

bool UPnP::begin() {
#ifndef NO_GATEWAYIP
  if (is_wifi) {
    gatewayIP = WiFi.gatewayIP();
    localIP = WiFi.localIP();
  }
  return true;
#endif
  if (localIP == NullIP) {
    DlnaLogger.log(DlnaError, "localIP is not defined");
    return false;
  }
  if (gatewayIP == NullIP) {
    DlnaLogger.log(DlnaError, "gatewayIP is not defined");
    return false;
  }
  return true;
}

bool UPnP::save() {
  if (ruleNodes.empty()) {
    DlnaLogger.log(DlnaError, "No UPnP port mapping was set.");
    lastResult = PortMappingResult::EMPTY_PORT_MAPPING_CONFIG;
    return false;
  }

  unsigned long startTime = millis();

  // verify WiFi is connected
  if (!checkConnectivity(startTime)) {
    DlnaLogger.log(DlnaError, "not connected to WiFi, cannot continue");
    lastResult = PortMappingResult::NETWORK_ERROR;
    return false;
  }

  // get all the needed IGD information using SSDP if we don't have it already
  if (!gatewayDlnaInfo) {
    getGatewayDlnaInfo(&gatewayDlnaInfo, startTime);
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaError, "Invalid router info, cannot continue");
      p_client->stop();
      lastResult = PortMappingResult::NETWORK_ERROR;
      return false;
    }
    delay(1000);  // longer delay to allow more time for the router to update
                  // its rules
  }

  // verify gateway information is valid
  if (!gatewayDlnaInfo) {
    DlnaLogger.log(DlnaError, "Invalid router info, cannot continue");
    lastResult = PortMappingResult::NETWORK_ERROR;
    return false;
  }

  DlnaLogger.log(DlnaInfo, "port [%d] actionPort [%d]", gatewayDlnaInfo.port,
                 gatewayDlnaInfo.actionPort);

  if (gatewayDlnaInfo.port != gatewayDlnaInfo.actionPort) {
    // in this case we need to connect to a different port
    DlnaLogger.log(DlnaError,
                   "Connection port changed, disconnecting from IGD");
    p_client->stop();
  }

  bool allPortMappingsAlreadyExist = true;  // for debug
  int addedPortMappings = 0;                // for debug
  for (auto &rule : ruleNodes) {
    DlnaLogger.log(DlnaInfo, "Verify port mapping for rule [%s]",
                   rule.devFriendlyName);
    bool currPortMappingAlreadyExists = true;  // for debug
    // TODO: since verifyPortMapping connects to the IGD then
    // addPortMappingEntry can skip it
    if (!verifyPortMapping(&gatewayDlnaInfo, &rule)) {
      // need to add the port mapping
      currPortMappingAlreadyExists = false;
      allPortMappingsAlreadyExist = false;
      if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
        DlnaLogger.log(DlnaError,
                       "Timeout expired while trying to add a port mapping");
        p_client->stop();
        lastResult = PortMappingResult::TIMEOUT;
        return false;
      }

      addPortMappingEntry(&gatewayDlnaInfo, &rule);

      int tries = 0;
      while (tries <= 3) {
        delay(2000);  // longer delay to allow more time for the router to
                      // update its rules
        if (verifyPortMapping(&gatewayDlnaInfo, &rule)) {
          break;
        }
        tries++;
      }

      if (tries > 3) {
        p_client->stop();
        lastResult = PortMappingResult::VERIFICATION_FAILED;
        return false;
      }
    }

    if (!currPortMappingAlreadyExists) {
      addedPortMappings++;
      DlnaLogger.log(DlnaInfo, "Port mapping [%s] was added",
                     rule.devFriendlyName);
    }
  }

  p_client->stop();

  if (allPortMappingsAlreadyExist) {
    DlnaLogger.log(
        DlnaInfo,
        "All port mappings were already found in the IGD, not doing anything");
    lastResult = PortMappingResult::ALREADY_MAPPED;
    return true;
  } else {
    // addedPortMappings is at least 1 here
    if (addedPortMappings > 1) {
      DlnaLogger.log(DlnaInfo, "%d UPnP port mappings were added",
                     addedPortMappings);
    } else {
      DlnaLogger.log(DlnaInfo, "One UPnP port mapping was added");
    }
  }
  lastResult = PortMappingResult::SUCCESS;
  return true;
}

bool UPnP::getGatewayDlnaInfo(GatewayDlnaInfo *deviceDlnaInfo, long startTime) {
  while (!connectUDP()) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaError, "Timeout expired while connecting UDP");
      p_udp->stop();
      return false;
    }
    delay(500);
  }

  broadcastMSearch();
  DlnaLogger.log(DlnaInfo, "Gateway IP [%s]", toStr(gatewayIP));

  SsdpDevice ssdpDevice;
  while ((ssdpDevice = waitForUnicastResponseToMSearch(gatewayIP)) == true) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaError,
                     "Timeout expired while waiting for the gateway router to "
                     "respond to M-SEARCH message");
      p_udp->stop();
      return false;
    }
    delay(1);
  }

  if (!ssdpDevice) return false;

  deviceDlnaInfo->host = ssdpDevice.host;
  deviceDlnaInfo->port = ssdpDevice.port;
  deviceDlnaInfo->path = ssdpDevice.path;
  // the following is the default and may be overridden if URLBase tag is
  // specified
  deviceDlnaInfo->actionPort = ssdpDevice.port;

  // close the UDP connection
  p_udp->stop();

  // connect to IGD (TCP connection)
  while (!connectToIGD(deviceDlnaInfo->host, deviceDlnaInfo->port)) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaError,
                     "Timeout expired while trying to connect to the IGD");
      p_client->stop();
      return false;
    }
    delay(500);
  }

  // get event urls from the gateway IGD
  while (!getIGDEventURLs(deviceDlnaInfo)) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaError,
                     "Timeout expired while adding a new port mapping");
      p_client->stop();
      return false;
    }
    delay(500);
  }

  return true;
}

bool UPnP::update(unsigned long intervalMs, callback_function fallback) {
  if (!isActive) return false;
  if (millis() - lastUpdateTime >= intervalMs) {
    DlnaLogger.log(DlnaInfo, "Updating port mapping");

    // fallback
    if (consequtiveFails >= MAX_NUM_OF_UPDATES_WITH_NO_EFFECT) {
      DlnaLogger.log(DlnaError,
                     "Too many times with no effect on updatePortMappings. "
                     "Current number of fallbacks times [%d]",
                     consequtiveFails);

      consequtiveFails = 0;
      gatewayDlnaInfo.clear();
      if (fallback != nullptr) {
        DlnaLogger.log(DlnaInfo, "Executing fallback method");
        fallback();
      }

      lastResult = PortMappingResult::TIMEOUT;
      return false;
    }

    bool result = begin();

    if (result) {
      lastUpdateTime = millis();
      p_client->stop();
      consequtiveFails = 0;
      return result;
    } else {
      lastUpdateTime += intervalMs / 2;  // delay next try
      DlnaLogger.log(DlnaError,
                     "While updating UPnP port mapping. Failed with error "
                     "code [%d]",
                     lastResult);
      p_client->stop();
      consequtiveFails++;
      return result;
    }
  }

  p_client->stop();
  lastResult = PortMappingResult::NOP;  // no need to check yet
  return true;
}

bool UPnP::checkConnectivity(unsigned long startTime) {
  if (is_wifi) {
    DlnaLogger.log(DlnaInfo, "Testing WiFi connection for [%s]",
                   toStr(localIP));
    while (WiFi.status() != WL_CONNECTED) {
      if (timeoutMs > 0 && startTime > 0 &&
          (millis() - startTime > timeoutMs)) {
        DlnaLogger.log(DlnaError,
                       " ==> Timeout expired while verifying WiFi connection");
        p_client->stop();
        return false;
      }
      delay(200);
    }
    DlnaLogger.log(DlnaInfo, " ==> GOOD");  // \n
  }

  DlnaLogger.log(DlnaInfo, "Testing internet connection");
  p_client->connect(connectivityTestIp, 80);
  while (!p_client->connected()) {
    if (startTime + TCP_CONNECTION_TIMEOUT_MS > millis()) {
      DlnaLogger.log(DlnaError, " ==> BAD");
      p_client->stop();
      return false;
    }
  }

  DlnaLogger.log(DlnaInfo, " ==> GOOD");
  p_client->stop();
  return true;
}

bool UPnP::verifyPortMapping(GatewayDlnaInfo *deviceDlnaInfo,
                             UpnpRule *rule_ptr) {
  if (!applyActionOnSpecificPortMapping(&SOAPActionGetSpecificPortMappingEntry,
                                        deviceDlnaInfo, rule_ptr)) {
    return false;
  }

  DlnaLogger.log(DlnaInfo, "verifyPortMapping called");

  // TODO: extract the current lease duration and return it instead of a bool
  bool isSuccess = false;
  bool detectedChangedIP = false;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    DlnaLogger.log(DlnaDebug, line.c_str());
    if (line.indexOf(F("errorCode")) >= 0) {
      isSuccess = false;
      // flush response and exit loop
      while (p_client->available()) {
        line = p_client->readStringUntil('\r');
        DlnaLogger.log(DlnaDebug, line.c_str());
      }
      continue;
    }

    if (line.indexOf(F("NewInternalClient")) >= 0) {
      String content = getTagContent(line, F("NewInternalClient"));
      if (content.length() > 0) {
        IPAddress ipAddressToVerify = (rule_ptr->internalAddr == NullIP)
                                          ? localIP
                                          : rule_ptr->internalAddr;
        if (content == String(ipAddressToVerify)) {
          isSuccess = true;
        } else {
          detectedChangedIP = true;
        }
      }
    }
  }

  p_client->stop();

  if (isSuccess) {
    DlnaLogger.log(DlnaInfo, "Port mapping found in IGD");
  } else if (detectedChangedIP) {
    DlnaLogger.log(DlnaInfo, "Detected a change in IP");
    removeAllPortMappingsFromIGD();
  } else {
    DlnaLogger.log(DlnaError, "Could not find port mapping in IGD");
  }

  return isSuccess;
}

bool UPnP::deletePortMapping(GatewayDlnaInfo *deviceDlnaInfo,
                             UpnpRule *rule_ptr) {
  if (!applyActionOnSpecificPortMapping(&SOAPActionDeletePortMapping,
                                        deviceDlnaInfo, rule_ptr)) {
    return false;
  }

  bool isSuccess = false;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    DlnaLogger.log(DlnaDebug, line.c_str());
    if (line.indexOf(F("errorCode")) >= 0) {
      isSuccess = false;
      // flush response and exit loop
      while (p_client->available()) {
        line = p_client->readStringUntil('\r');
        DlnaLogger.log(DlnaDebug, line.c_str());
      }
      continue;
    }
    if (line.indexOf(F("DeletePortMappingResponse")) >= 0) {
      isSuccess = true;
    }
  }

  return isSuccess;
}

bool UPnP::applyActionOnSpecificPortMapping(SOAPAction *soapAction,
                                            GatewayDlnaInfo *deviceDlnaInfo,
                                            UpnpRule *rule_ptr) {
  UpnpRule &rule = *rule_ptr;
  char integer_string[32];
  DlnaLogger.log(DlnaInfo, "Apply action [%d] on port mapping [%s]",
                 soapAction->name, rule.devFriendlyName);

  // connect to IGD (TCP connection) again, if needed, in case we got
  // disconnected after the previous query
  unsigned long timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  if (!p_client->connected()) {
    while (!connectToIGD(deviceDlnaInfo->host, deviceDlnaInfo->actionPort)) {
      if (millis() > timeout) {
        DlnaLogger.log(DlnaError,
                       "Timeout expired while trying to connect to the IGD");
        p_client->stop();
        return false;
      }
      delay(500);
    }
  }

  strcpy_P(body_tmp,
           PSTR("<?xml version=\"1.0\"?>\r\n<s:Envelope "
                "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/"
                "\">\r\n<s:Body>\r\n<u:"));
  strcat_P(body_tmp, soapAction->name);
  strcat_P(body_tmp, PSTR(" xmlns:u=\""));
  strcat_P(body_tmp, deviceDlnaInfo->serviceTypeName.c_str());
  strcat_P(body_tmp,
           PSTR("\">\r\n<NewRemoteHost></NewRemoteHost>\r\n<NewExternalPort>"));
  sprintf(integer_string, "%d", rule_ptr->externalPort);
  strcat_P(body_tmp, integer_string);
  strcat_P(body_tmp, PSTR("</NewExternalPort>\r\n<NewProtocol>"));
  strcat_P(body_tmp, rule.protocol.c_str());
  strcat_P(body_tmp, PSTR("</NewProtocol>\r\n</u:"));
  strcat_P(body_tmp, soapAction->name);
  strcat_P(body_tmp, PSTR(">\r\n</s:Body>\r\n</s:Envelope>\r\n"));

  sprintf(integer_string, "%d", (int)strlen(body_tmp));

  p_client->print(F("POST "));

  p_client->print(deviceDlnaInfo->actionPath);
  p_client->println(F(" HTTP/1.1"));
  p_client->println(F("Connection: close"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  p_client->println(String("Host: ") + toStr(deviceDlnaInfo->host) + ":" +
                    String(deviceDlnaInfo->actionPort));
  p_client->print(F("SOAPAction: \""));
  p_client->print(deviceDlnaInfo->serviceTypeName);
  p_client->print(F("#"));
  p_client->print(soapAction->name);
  p_client->println(F("\""));
  p_client->print(F("Content-Length: "));
  p_client->println(integer_string);
  p_client->println();

  p_client->println(body_tmp);
  p_client->println();

  DlnaLogger.log(DlnaDebug, body_tmp);

  timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  while (p_client->available() == 0) {
    if (millis() > timeout) {
      DlnaLogger.log(DlnaError,
                     "TCP connection timeout while retrieving port mappings");
      p_client->stop();
      // TODO: in this case we might not want to add the ports right away
      // might want to try again or only start adding the ports after we
      // definitely did not see them in the router list
      return false;
    }
  }
  return true;
}

void UPnP::removeAllPortMappingsFromIGD() { ruleNodes.clear(); }

// a single try to connect UDP multicast address and port of UPnP
// (239.255.255.250 and 1900 respectively) this will enable receiving SSDP
// packets after the M-SEARCH multicast message will be broadcasted
bool UPnP::connectUDP() {
#if defined(ESP8266)
  if (p_udp->beginMulticast(localIP, ipMulti, 0)) {
    return true;
  }
#else
  if (p_udp->beginMulticast(ipMulti, UPNP_SSDP_PORT)) {
    return true;
  }
#endif

  DlnaLogger.log(DlnaError, "UDP connection failed");
  return false;
}

// broadcast an M-SEARCH message to initiate messages from SSDP devices
// the router should respond to this message by a packet sent to this device's
// unicast addresss on the same UPnP port (1900)
void UPnP::broadcastMSearch(bool isSsdpAll /*=false*/) {
  char integer_string[32];

  DlnaLogger.log(DlnaInfo, "Sending M-SEARCH to [%s] Port [%d]", toStr(ipMulti),
                 UPNP_SSDP_PORT);

#if defined(ESP8266)
  p_udp->beginPacketMulticast(ipMulti, UPNP_SSDP_PORT, localIP);
#else
  uint8_t beginMulticastPacketRes = p_udp->beginPacket(ipMulti, UPNP_SSDP_PORT);
  DlnaLogger.log(DlnaInfo, "beginMulticastPacketRes [%d]",
                 beginMulticastPacketRes);
#endif

  const char *const *deviceList = deviceListUpnp;
  if (isSsdpAll) {
    deviceList = deviceListSsdpAll;
  }

  for (int i = 0; deviceList[i]; i++) {
    strcpy_P(body_tmp, PSTR("M-SEARCH * HTTP/1.1\r\n"));
    strcat_P(body_tmp, PSTR("HOST: 239.255.255.250:"));
    sprintf(integer_string, "%d", UPNP_SSDP_PORT);
    strcat_P(body_tmp, integer_string);
    strcat_P(body_tmp, PSTR("\r\n"));
    strcat_P(body_tmp, PSTR("MAN: \"ssdp:discover\"\r\n"));
    strcat_P(body_tmp, PSTR("MX: 2\r\n"));  // allowed number of seconds to wait
                                            // before replying to this M_SEARCH
    strcat_P(body_tmp, PSTR("ST: "));
    strcat_P(body_tmp, deviceList[i]);
    strcat_P(body_tmp, PSTR("\r\n"));
    strcat_P(body_tmp, PSTR("USER-AGENT: unix/5.1 UPnP/2.0 UPnP/1.0\r\n"));
    strcat_P(body_tmp, PSTR("\r\n"));

    DlnaLogger.log(DlnaInfo, body_tmp);
    size_t len = strlen(body_tmp);
    DlnaLogger.log(DlnaInfo, "M-SEARCH packet length is [%d]", len);

#if defined(ESP8266)
    p_udp->write(body_tmp);
#else
    p_udp->print(body_tmp);
#endif

    int endPacketRes = p_udp->endPacket();
    DlnaLogger.log(DlnaInfo, "endPacketRes [%d]", endPacketRes);
  }

  DlnaLogger.log(DlnaInfo, "M-SEARCH packets sent");
}

Vector<SsdpDevice> UPnP::listDevices() {
  devices.clear();
  if (timeoutMs <= 0) {
    DlnaLogger.log(
        DlnaInfo,
        "Timeout must be set when initializing UPnP to use this method, "
        "exiting.");
    return devices;
  }

  unsigned long startTime = millis();
  while (!connectUDP()) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(DlnaInfo, "Timeout expired while connecting UDP");
      p_udp->stop();
      return devices;
    }
    delay(500);
  }

  broadcastMSearch(true);
  DlnaLogger.log(DlnaInfo, "Gateway IP [%s]", toStr(gatewayIP));

  SsdpDevice ssdpDevice;
  while (true) {
    // nullptr will cause finding all SSDP device (not just the IGD)
    ssdpDevice = waitForUnicastResponseToMSearch(NullIP);
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      DlnaLogger.log(
          DlnaInfo,
          "Timeout expired while waiting for the gateway router to respond "
          "to M-SEARCH message");
      p_udp->stop();
      break;
    }

    // ssdpDeviceToString(ssdpDevice_ptr);
    if (ssdpDevice) {
      if (!devices.contains(ssdpDevice)) devices.push_back(ssdpDevice);
    }

    delay(5);
  }

  // close the UDP connection
  p_udp->stop();

  return devices;
}

// Assuming an M-SEARCH message was broadcaseted, wait for the response from the
// IGD (Internet Gateway Device) Note: the response from the IGD is sent back as
// unicast to this device Note: only gateway defined IGD response will be
// considered, the rest will be ignored
SsdpDevice UPnP::waitForUnicastResponseToMSearch(IPAddress gatewayIP) {
  SsdpDevice newSsdpDevice;
  // Flush the UDP buffer since otherwise anyone who responded first will be the
  // only one we see and we will not see the response from the gateway router
  p_udp->flush();
  int packetSize = p_udp->parsePacket();

  // only continue if a packet is available
  if (packetSize <= 0) {
    return newSsdpDevice;
  }

  IPAddress remoteIP = p_udp->remoteIP();
  // only continue if the packet was received from the gateway router
  // for SSDP discovery we continue anyway
  if (!(gatewayIP == NullIP) && !(remoteIP == gatewayIP)) {
    DlnaLogger.log(DlnaInfo,
                   "Discarded packet not originating from IGD - gatewayIP [%s] "
                   "remoteIP [%s]",
                   toStr(gatewayIP), toStr(remoteIP));
    return newSsdpDevice;
  }

  DlnaLogger.log(DlnaInfo, "Received packet of size [%d] ip [%s] ] port [%d]",
                 packetSize, toStr(remoteIP), p_udp->remotePort());

  // sanity check
  if (packetSize > UPNP_UDP_TX_RESPONSE_MAX_SIZE) {
    (DlnaLogger.log(
        DlnaError,
        "Received packet with size larged than the response buffer, cannot "
        "proceed."));
    return newSsdpDevice;
  }

  int idx = 0;
  while (idx < packetSize) {
    memset(packetBuffer, 0, UPNP_UDP_TX_PACKET_MAX_SIZE);
    int len = p_udp->read(packetBuffer, UPNP_UDP_TX_PACKET_MAX_SIZE);
    if (len <= 0) {
      break;
    }
    DlnaLogger.log(DlnaInfo, "UDP packet read bytes [%d]  out of [%d]", len,
                   packetSize);
    memcpy(responseBuffer + idx, packetBuffer, len);
    idx += len;
  }
  responseBuffer[idx] = '\0';

  DlnaLogger.log(DlnaInfo, "Gateway packet content: %s", responseBuffer);

  const char *const *deviceList = deviceListUpnp;
  if (gatewayIP == NullIP) {
    deviceList = deviceListSsdpAll;
  }

  // only continue if the packet is a response to M-SEARCH and it originated
  // from a gateway device for SSDP discovery we continue anyway
  if (gatewayIP != NullIP) {  // for the use of listDevices
    bool foundIGD = false;
    for (int i = 0; deviceList[i]; i++) {
      if (strstr(responseBuffer, deviceList[i]) != nullptr) {
        foundIGD = true;
        DlnaLogger.log(DlnaInfo, "IGD of type [%s] found", deviceList[i]);
        break;
      }
    }

    if (!foundIGD) {
      DlnaLogger.log(DlnaWarning, "IGD was not found");
      return newSsdpDevice;
    }
  }

  String location = "";
  char *location_indexStart = strstr(responseBuffer, "location:");
  if (location_indexStart == nullptr) {
    location_indexStart = strstr(responseBuffer, "Location:");
  }
  if (location_indexStart == nullptr) {
    location_indexStart = strstr(responseBuffer, "LOCATION:");
  }
  if (location_indexStart != nullptr) {
    location_indexStart += 9;  // "location:".length()
    char *location_indexEnd = strstr(location_indexStart, "\r\n");
    if (location_indexEnd != nullptr) {
      int urlLength = location_indexEnd - location_indexStart;
      int arrLength = urlLength + 1;  // + 1 for '\0'
      // converting the start index to be inside the packetBuffer rather than
      // responseBuffer
      char locationCharArr[arrLength];
      memcpy(locationCharArr, location_indexStart, urlLength);
      locationCharArr[arrLength - 1] = '\0';
      location = String(locationCharArr);
      location.trim();
    } else {
      DlnaLogger.log(DlnaError, "could not extract value from LOCATION param");
      return newSsdpDevice;
    }
  } else {
    DlnaLogger.log(DlnaError, "LOCATION param was not found");
    return newSsdpDevice;
  }

  DlnaLogger.log(DlnaInfo, "Device location found [%s]", location.c_str());

  IPAddress host = getHost(location);
  int port = getPort(location);
  String path = getPath(location);

  newSsdpDevice.host = host;
  newSsdpDevice.port = port;
  newSsdpDevice.path = path;

  return newSsdpDevice;
}

// a single trial to connect to the IGD (with TCP)
bool UPnP::connectToIGD(IPAddress host, int port) {
  DlnaLogger.log(DlnaInfo, "Connecting to IGD with host [%s] port [%d] ",
                 toStr(host), port);
  if (p_client->connect(host, port)) {
    DlnaLogger.log(DlnaInfo, "Connected to IGD");
    return true;
  }
  return false;
}

// updates deviceDlnaInfo with the commands' information of the IGD
bool UPnP::getIGDEventURLs(GatewayDlnaInfo *deviceDlnaInfo) {
  DlnaLogger.log(DlnaInfo,
                 "called getIGDEventURLs deviceDlnaInfo->actionPath [%s] "
                 "deviceDlnaInfo->path [%s]",
                 deviceDlnaInfo->actionPath, deviceDlnaInfo->path);

  // make an HTTP request
  p_client->print(F("GET "));
  p_client->print(deviceDlnaInfo->path);
  p_client->println(F(" HTTP/1.1"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  // p_client->println(F("Connection: close"));
  p_client->println(String("Host: ") + toStr(deviceDlnaInfo->host) + ":" +
                    String(deviceDlnaInfo->actionPort));
  p_client->println(F("Content-Length: 0"));
  p_client->println();

  // wait for the response
  unsigned long timeout = millis();
  while (p_client->available() == 0) {
    if (millis() - timeout > TCP_CONNECTION_TIMEOUT_MS) {
      DlnaLogger.log(DlnaError,
                     "TCP connection timeout while executing getIGDEventURLs");
      p_client->stop();
      return false;
    }
  }

  // read all the lines of the reply from server
  bool upnpServiceFound = false;
  bool urlBaseFound = false;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    int index_in_line = 0;
    DlnaLogger.log(DlnaDebug, line.c_str());
    if (!urlBaseFound && line.indexOf(F("<URLBase>")) >= 0) {
      // e.g. <URLBase>http://192.168.1.1:5432/</URLBase>
      // Note: assuming URL path will only be found in a specific action under
      // the 'controlURL' xml tag
      String baseUrl = getTagContent(line, "URLBase");
      if (baseUrl.length() > 0) {
        baseUrl.trim();
        IPAddress host = getHost(baseUrl);  // this is ignored, assuming router
                                            // host IP will not change
        int port = getPort(baseUrl);
        deviceDlnaInfo->actionPort = port;

        DlnaLogger.log(DlnaInfo, "URLBase tag found [%s]", baseUrl);
        DlnaLogger.log(DlnaInfo,
                       "Translated to base host [%s] and base port [%d]",
                       toStr(host), port);
        urlBaseFound = true;
      }
    }

    // to support multiple <serviceType> tags
    int service_type_index_start = 0;

    for (int i = 0; deviceListUpnp[i]; i++) {
      int service_type_index =
          line.indexOf(String(UPNP_SERVICE_TYPE_TAG_START) + deviceListUpnp[i]);
      if (service_type_index >= 0) {
        DlnaLogger.log(DlnaInfo, "[%s] service_type_index [%d]",
                       deviceDlnaInfo->serviceTypeName, service_type_index);
        service_type_index_start = service_type_index;
        service_type_index =
            line.indexOf(UPNP_SERVICE_TYPE_TAG_END, service_type_index_start);
      }
      if (!upnpServiceFound && service_type_index >= 0) {
        index_in_line += service_type_index;
        upnpServiceFound = true;
        deviceDlnaInfo->serviceTypeName =
            getTagContent(line.substring(service_type_index_start),
                          UPNP_SERVICE_TYPE_TAG_NAME);
        DlnaLogger.log(DlnaInfo, "[%s] service found! deviceType [%s]",
                       deviceDlnaInfo->serviceTypeName, deviceListUpnp[i]);
        break;  // will start looking for 'controlURL' now
      }
    }

    if (upnpServiceFound &&
        (index_in_line = line.indexOf("<controlURL>", index_in_line)) >= 0) {
      String controlURLContent =
          getTagContent(line.substring(index_in_line), "controlURL");
      if (controlURLContent.length() > 0) {
        deviceDlnaInfo->actionPath = controlURLContent;
        DlnaLogger.log(DlnaInfo,
                       "controlURL tag found! setting actionPath to [%s]",
                       controlURLContent);

        // clear buffer
        DlnaLogger.log(DlnaInfo, "Flushing the rest of the response");
        while (p_client->available()) {
          p_client->read();
        }

        // now we have (upnpServiceFound && controlURLFound)
        return true;
      }
    }
  }

  return false;
}

// assuming a connection to the IGD has been formed
// will add the port mapping to the IGD
bool UPnP::addPortMappingEntry(GatewayDlnaInfo *deviceDlnaInfo,
                               UpnpRule *rule_ptr) {
  UpnpRule &rule = *rule_ptr;
  char integer_string[32];
  DlnaLogger.log(DlnaInfo, "called addPortMappingEntry");

  // connect to IGD (TCP connection) again, if needed, in case we got
  // disconnected after the previous query
  unsigned long timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  if (!p_client->connected()) {
    while (!connectToIGD(gatewayDlnaInfo.host, gatewayDlnaInfo.actionPort)) {
      if (millis() > timeout) {
        DlnaLogger.log(DlnaError,
                       "Timeout expired while trying to connect to the IGD");
        p_client->stop();
        return false;
      }
      delay(500);
    }
  }

  DlnaLogger.log(DlnaInfo, "deviceDlnaInfo->actionPath [%s]",
                 deviceDlnaInfo->actionPath);
  DlnaLogger.log(DlnaInfo, "deviceDlnaInfo->serviceTypeName [%s]",
                 deviceDlnaInfo->serviceTypeName);

  strcpy_P(body_tmp,
           PSTR("<?xml version=\"1.0\"?><s:Envelope "
                "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/"
                "\"><s:Body><u:AddPortMapping xmlns:u=\""));
  strcat_P(body_tmp, deviceDlnaInfo->serviceTypeName.c_str());
  strcat_P(body_tmp,
           PSTR("\"><NewRemoteHost></NewRemoteHost><NewExternalPort>"));
  sprintf(integer_string, "%d", rule.externalPort);
  strcat_P(body_tmp, integer_string);
  strcat_P(body_tmp, PSTR("</NewExternalPort><NewProtocol>"));
  strcat_P(body_tmp, rule.protocol.c_str());
  strcat_P(body_tmp, PSTR("</NewProtocol><NewInternalPort>"));
  sprintf(integer_string, "%d", rule.internalPort);
  strcat_P(body_tmp, integer_string);
  strcat_P(body_tmp, PSTR("</NewInternalPort><NewInternalClient>"));
  IPAddress ipAddress =
      (rule.internalAddr == NullIP) ? localIP : rule.internalAddr;
  strcat_P(body_tmp, toStr(ipAddress));
  strcat_P(body_tmp, PSTR("</NewInternalClient><NewEnabled>1</"
                          "NewEnabled><NewPortMappingDescription>"));
  strcat_P(body_tmp, rule.devFriendlyName.c_str());
  strcat_P(body_tmp, PSTR("</NewPortMappingDescription><NewLeaseDuration>"));
  sprintf(integer_string, "%d", rule.leaseDurationSec);
  strcat_P(body_tmp, integer_string);
  strcat_P(
      body_tmp,
      PSTR("</NewLeaseDuration></u:AddPortMapping></s:Body></s:Envelope>"));

  sprintf(integer_string, "%d", (int)strlen(body_tmp));

  p_client->print(F("POST "));
  p_client->print(deviceDlnaInfo->actionPath);
  p_client->println(F(" HTTP/1.1"));
  // p_client->println(F("Connection: close"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  p_client->println(String("Host: ") + toStr(deviceDlnaInfo->host) + ":" +
                    String(deviceDlnaInfo->actionPort));
  // p_client->println(F("Accept: */*"));
  // p_client->println(F("Content-Type: application/x-www-form-urlencoded"));
  p_client->print(F("SOAPAction: \""));
  p_client->print(deviceDlnaInfo->serviceTypeName);
  p_client->println(F("#AddPortMapping\""));

  p_client->print(F("Content-Length: "));
  p_client->println(integer_string);
  p_client->println();

  p_client->println(body_tmp);
  p_client->println();

  DlnaLogger.log(DlnaInfo, "Content-Length was: %s", integer_string);
  DlnaLogger.log(DlnaDebug, body_tmp);

  timeout = millis();
  while (p_client->available() == 0) {
    if (millis() - timeout > TCP_CONNECTION_TIMEOUT_MS) {
      DlnaLogger.log(DlnaError,
                     "TCP connection timeout while adding a port mapping");
      p_client->stop();
      return false;
    }
  }

  // TODO: verify success
  bool isSuccess = true;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    if (line.indexOf(F("errorCode")) >= 0) {
      isSuccess = false;
    }
    DlnaLogger.log(DlnaDebug, line.c_str());
  }

  if (!isSuccess) {
    p_client->stop();
  }

  return isSuccess;
}

bool UPnP::updatePortMappings() {
  ruleNodes.clear();
  char integer_string[32];
  // verify gateway information is valid
  // TODO: use this _gwDlnaInfo to skip the UDP part completely if it is not
  // empty
  if (!gatewayDlnaInfo) {
    DlnaLogger.log(DlnaError, "Invalid router info, cannot continue");
    return false;
  }

  unsigned long startTime = millis();
  bool reachedEnd = false;
  int index = 0;
  while (!reachedEnd) {
    // connect to IGD (TCP connection) again, if needed, in case we got
    // disconnected after the previous query
    unsigned long timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
    if (!p_client->connected()) {
      while (!connectToIGD(gatewayDlnaInfo.host, gatewayDlnaInfo.actionPort)) {
        if (millis() > timeout) {
          DlnaLogger.log(DlnaError,
                         "Timeout expired while trying to connect to the IGD");
          p_client->stop();
          ruleNodes.clear();
          return false;
        }
        delay(1000);
      }
    }

    DlnaLogger.log(DlnaInfo, "Sending query for index [%d]", index);

    strcpy_P(
        body_tmp,
        PSTR(
            "<?xml version=\"1.0\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
            "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
            "<u:GetGenericPortMappingEntry xmlns:u=\""));
    strcat_P(body_tmp, gatewayDlnaInfo.serviceTypeName.c_str());
    strcat_P(body_tmp, PSTR("\">"
                            "  <NewPortMappingIndex>"));

    sprintf(integer_string, "%d", index);
    strcat_P(body_tmp, integer_string);
    strcat_P(body_tmp, PSTR("</NewPortMappingIndex>"
                            "</u:GetGenericPortMappingEntry>"
                            "</s:Body>"
                            "</s:Envelope>"));

    sprintf(integer_string, "%d", (int)strlen(body_tmp));

    p_client->print(F("POST "));
    p_client->print(gatewayDlnaInfo.actionPath);
    p_client->println(F(" HTTP/1.1"));
    p_client->println(F("Connection: keep-alive"));
    p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
    p_client->println(String("Host: ") + toStr(gatewayDlnaInfo.host) + ":" +
                      String(gatewayDlnaInfo.actionPort));
    p_client->print(F("SOAPAction: \""));
    p_client->print(gatewayDlnaInfo.serviceTypeName);
    p_client->println(F("#GetGenericPortMappingEntry\""));

    p_client->print(F("Content-Length: "));
    p_client->println(integer_string);
    p_client->println();

    p_client->println(body_tmp);
    p_client->println();

    timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
    while (p_client->available() == 0) {
      if (millis() > timeout) {
        DlnaLogger.log(DlnaError,
                       "TCP connection timeout while retrieving port mappings");
        p_client->stop();
        ruleNodes.clear();
        return false;
      }
    }

    while (p_client->available()) {
      String line = p_client->readStringUntil('\r');
      DlnaLogger.log(DlnaDebug, line.c_str());
      if (line.indexOf(PORT_MAPPING_INVALID_INDEX) >= 0) {
        reachedEnd = true;
      } else if (line.indexOf(PORT_MAPPING_INVALID_ACTION) >= 0) {
        DlnaLogger.log(DlnaError, "Invalid action while reading port mappings");
        reachedEnd = true;
      } else if (line.indexOf(F("HTTP/1.1 500 ")) >= 0) {
        DlnaLogger.log(
            DlnaError,
            "Internal server error, likely because we have shown all the "
            "mappings");
        reachedEnd = true;
      } else if (line.indexOf(F("GetGenericPortMappingEntryResponse")) >= 0) {
        UpnpRule rule;
        rule.index = index;
        rule.devFriendlyName = getTagContent(line, "NewPortMappingDescription");
        String newInternalClient = getTagContent(line, "NewInternalClient");
        if (newInternalClient == "") {
          continue;
        }
        rule.internalAddr.fromString(newInternalClient);
        rule.internalPort = getTagContent(line, "NewInternalPort").toInt();
        rule.externalPort = getTagContent(line, "NewExternalPort").toInt();
        rule.protocol = getTagContent(line, "NewProtocol");
        rule.leaseDurationSec = getTagContent(line, "NewLeaseDuration").toInt();

        ruleNodes.push_back(rule);
      }
    }

    index++;
    delay(250);
  }
  p_client->stop();
  return ruleNodes;
}

Vector<UpnpRule> UPnP::listConfig() { return ruleNodes; }

void UPnP::logSsdpDevice(SsdpDevice *ssdpDevice) {
  DlnaLogger.log(DlnaInfo, "SSDP device [%s] port [%d] path [%s]",
                 toStr(ssdpDevice->host), ssdpDevice->port, ssdpDevice->path);
}

const char *UPnP::spaces(int num) {
  static char result[80] = {0};
  if (num < 0) num = 1;
  if (num >= 78) num = 78;
  for (int j = 0; j < num; j++) {
    result[j] = ' ';
  }
  return result;
}

IPAddress UPnP::getHost(String url) {
  IPAddress result(0, 0, 0, 0);
  if (url.indexOf(F("https://")) != -1) {
    url.replace("https://", "");
  }
  if (url.indexOf(F("http://")) != -1) {
    url.replace("http://", "");
  }
  int endIndex = url.indexOf('/');
  if (endIndex != -1) {
    url = url.substring(0, endIndex);
  }
  int colonsIndex = url.indexOf(':');
  if (colonsIndex != -1) {
    url = url.substring(0, colonsIndex);
  }
  result.fromString(url);
  return result;
}

int UPnP::getPort(String url) {
  int port = -1;
  if (url.indexOf(F("https://")) != -1) {
    url.replace("https://", "");
  }
  if (url.indexOf(F("http://")) != -1) {
    url.replace("http://", "");
  }
  int portEndIndex = url.indexOf("/");
  if (portEndIndex == -1) {
    portEndIndex = url.length();
  }
  url = url.substring(0, portEndIndex);
  int colonsIndex = url.indexOf(":");
  if (colonsIndex != -1) {
    url = url.substring(colonsIndex + 1, portEndIndex);
    port = url.toInt();
  } else {
    port = 80;
  }
  return port;
}

String UPnP::getPath(String url) {
  if (url.indexOf(F("https://")) != -1) {
    url.replace("https://", "");
  }
  if (url.indexOf(F("http://")) != -1) {
    url.replace("http://", "");
  }
  int firstSlashIndex = url.indexOf("/");
  if (firstSlashIndex == -1) {
    DlnaLogger.log(DlnaError, "Cannot find path in url [%s]", url);
    return "";
  }
  return url.substring(firstSlashIndex, url.length());
}

String UPnP::getTagContent(const String &line, String tagName) {
  int startIndex = line.indexOf("<" + tagName + ">");
  if (startIndex == -1) {
    DlnaLogger.log(DlnaError,
                   "Cannot find tag content in line [%s] for start tag [<%s>]",
                   line, tagName);
    return "";
  }
  startIndex += tagName.length() + 2;
  int endIndex = line.indexOf("</" + tagName + ">", startIndex);
  if (endIndex == -1) {
    DlnaLogger.log(DlnaError,
                   "Cannot find tag content in line [] for end tag [<>]", line,
                   tagName);
    return "";
  }
  return line.substring(startIndex, endIndex);
}

}  // namespace tiny_dlna