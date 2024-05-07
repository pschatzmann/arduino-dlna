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

void UPnP::addConfig(IPAddress ruleIP, int ruleInternalPort,
                     int ruleExternalPort, const char *ruleProtocol,
                     int ruleLeaseDurationSec, const char *ruleFriendlyName) {
  static int index = 0;
  UpnpRule rule;
  rule.index = index++;
  rule.internalAddr = (ruleIP == WiFi.localIP())
                          ? ipNull
                          : ruleIP;  // for automatic IP change handling
  rule.internalPort = ruleInternalPort;
  rule.externalPort = ruleExternalPort;
  rule.leaseDurationSec = ruleLeaseDurationSec;
  rule.protocol = ruleProtocol;
  rule.devFriendlyName = ruleFriendlyName;

  ruleNodes.push_back(rule);
}

bool UPnP::begin() {
  if (ruleNodes.empty()) {
    debugPrintln(F("ERROR: No UPnP port mapping was set."));
    lastResult = PortMappingResult::EMPTY_PORT_MAPPING_CONFIG;
    return false;
  }

  unsigned long startTime = millis();

  // verify WiFi is connected
  if (!checkConnectivity(startTime)) {
    debugPrintln(F("ERROR: not connected to WiFi, cannot continue"));
    lastResult = PortMappingResult::NETWORK_ERROR;
    return false;
  }

  // get all the needed IGD information using SSDP if we don't have it already
  if (!gatewayInfo) {
    getGatewayInfo(&gatewayInfo, startTime);
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrintln(F("ERROR: Invalid router info, cannot continue"));
      p_client->stop();
      lastResult = PortMappingResult::NETWORK_ERROR;
      return false;
    }
    delay(1000);  // longer delay to allow more time for the router to update
                  // its rules
  }

  debugPrint(F("port ["));
  debugPrint(String(gatewayInfo.port));
  debugPrint(F("] actionPort ["));
  debugPrint(String(gatewayInfo.actionPort));
  debugPrintln(F("]"));

  // double verify gateway information is valid
  if (!gatewayInfo) {
    debugPrintln(F("ERROR: Invalid router info, cannot continue"));
    lastResult = PortMappingResult::NETWORK_ERROR;
    return false;
  }

  if (gatewayInfo.port != gatewayInfo.actionPort) {
    // in this case we need to connect to a different port
    debugPrintln(F("Connection port changed, disconnecting from IGD"));
    p_client->stop();
  }

  bool allPortMappingsAlreadyExist = true;  // for debug
  int addedPortMappings = 0;                // for debug
  for (auto &rule : ruleNodes) {
    debugPrint(F("Verify port mapping for rule ["));
    debugPrint(rule.devFriendlyName);
    debugPrintln(F("]"));
    bool currPortMappingAlreadyExists = true;  // for debug
    // TODO: since verifyPortMapping connects to the IGD then
    // addPortMappingEntry can skip it
    if (!verifyPortMapping(&gatewayInfo, &rule)) {
      // need to add the port mapping
      currPortMappingAlreadyExists = false;
      allPortMappingsAlreadyExist = false;
      if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
        debugPrintln(F("Timeout expired while trying to add a port mapping"));
        p_client->stop();
        lastResult = PortMappingResult::TIMEOUT;
        return false;
      }

      addPortMappingEntry(&gatewayInfo, &rule);

      int tries = 0;
      while (tries <= 3) {
        delay(2000);  // longer delay to allow more time for the router to
                      // update its rules
        if (verifyPortMapping(&gatewayInfo, &rule)) {
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
      debugPrint(F("Port mapping ["));
      debugPrint(rule.devFriendlyName);
      debugPrintln(F("] was added"));
    }
  }

  p_client->stop();

  if (allPortMappingsAlreadyExist) {
    debugPrintln(F(
        "All port mappings were already found in the IGD, not doing anything"));
    lastResult = PortMappingResult::ALREADY_MAPPED;
    return true;
  } else {
    // addedPortMappings is at least 1 here
    if (addedPortMappings > 1) {
      debugPrint(addedPortMappings);
      debugPrintln(F(" UPnP port mappings were added"));
    } else {
      debugPrintln(F("One UPnP port mapping was added"));
    }
  }
  lastResult = PortMappingResult::SUCCESS;
  return true;
}

bool UPnP::getGatewayInfo(GatewayInfo *deviceInfo, long startTime) {
  while (!connectUDP()) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrint(F("Timeout expired while connecting UDP"));
      p_udp->stop();
      return false;
    }
    delay(500);
    debugPrint(".");
  }
  debugPrintln("");  // \n

  broadcastMSearch();
  IPAddress gatewayIP = WiFi.gatewayIP();

  debugPrint(F("Gateway IP ["));
  debugPrint(gatewayIP.toString());
  debugPrintln(F("]"));

  SsdpDevice ssdpDevice;
  while ((ssdpDevice = waitForUnicastResponseToMSearch(gatewayIP)) == true) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrintln(
          F("Timeout expired while waiting for the gateway router to respond "
            "to M-SEARCH message"));
      p_udp->stop();
      return false;
    }
    delay(1);
  }

  deviceInfo->host = ssdpDevice.host;
  deviceInfo->port = ssdpDevice.port;
  deviceInfo->path = ssdpDevice.path;
  // the following is the default and may be overridden if URLBase tag is
  // specified
  deviceInfo->actionPort = ssdpDevice.port;

  // close the UDP connection
  p_udp->stop();

  // connect to IGD (TCP connection)
  while (!connectToIGD(deviceInfo->host, deviceInfo->port)) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrintln(F("Timeout expired while trying to connect to the IGD"));
      p_client->stop();
      return false;
    }
    delay(500);
  }

  // get event urls from the gateway IGD
  while (!getIGDEventURLs(deviceInfo)) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrintln(F("Timeout expired while adding a new port mapping"));
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
    debugPrintln(F("Updating port mapping"));

    // fallback
    if (consequtiveFails >= MAX_NUM_OF_UPDATES_WITH_NO_EFFECT) {
      debugPrint(
          F("ERROR: Too many times with no effect on updatePortMappings. "
            "Current number of fallbacks times ["));
      debugPrint(String(consequtiveFails));
      debugPrintln(F("]"));

      consequtiveFails = 0;
      gatewayInfo.clear();
      if (fallback != nullptr) {
        debugPrintln(F("Executing fallback method"));
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
      debugPrint(F(
          "ERROR: While updating UPnP port mapping. Failed with error code ["));
      debugPrint(String(result));
      debugPrintln(F("]"));
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
  debugPrint(F("Testing WiFi connection for ["));
  // debugPrint(WiFi.localIP().toString());
  // debugPrint("]");
  // while (WiFi.status() != WL_CONNECTED) {
  //   if (timeoutMs > 0 && startTime > 0 && (millis() - startTime > timeoutMs))
  //   {
  //     debugPrint(F(" ==> Timeout expired while verifying WiFi connection"));
  //     p_client->stop();
  //     return false;
  //   }
  //   delay(200);
  //   debugPrint(".");
  // }
  // debugPrintln(F(" ==> GOOD"));  // \n

  debugPrint(F("Testing internet connection"));
  p_client->connect(connectivityTestIp, 80);
  while (!p_client->connected()) {
    if (startTime + TCP_CONNECTION_TIMEOUT_MS > millis()) {
      debugPrintln(F(" ==> BAD"));
      p_client->stop();
      return false;
    }
  }

  debugPrintln(F(" ==> GOOD"));
  p_client->stop();
  return true;
}

bool UPnP::verifyPortMapping(GatewayInfo *deviceInfo, UpnpRule *rule_ptr) {
  if (!applyActionOnSpecificPortMapping(&SOAPActionGetSpecificPortMappingEntry,
                                        deviceInfo, rule_ptr)) {
    return false;
  }

  debugPrintln(F("verifyPortMapping called"));

  // TODO: extract the current lease duration and return it instead of a bool
  bool isSuccess = false;
  bool detectedChangedIP = false;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    debugPrint(line);
    if (line.indexOf(F("errorCode")) >= 0) {
      isSuccess = false;
      // flush response and exit loop
      while (p_client->available()) {
        line = p_client->readStringUntil('\r');
        debugPrint(line);
      }
      continue;
    }

    if (line.indexOf(F("NewInternalClient")) >= 0) {
      String content = getTagContent(line, F("NewInternalClient"));
      if (content.length() > 0) {
        IPAddress ipAddressToVerify = (rule_ptr->internalAddr == ipNull)
                                          ? WiFi.localIP()
                                          : rule_ptr->internalAddr;
        if (content == ipAddressToVerify.toString()) {
          isSuccess = true;
        } else {
          detectedChangedIP = true;
        }
      }
    }
  }

  debugPrintln("");  // \n

  p_client->stop();

  if (isSuccess) {
    debugPrintln(F("Port mapping found in IGD"));
  } else if (detectedChangedIP) {
    debugPrintln(F("Detected a change in IP"));
    removeAllPortMappingsFromIGD();
  } else {
    debugPrintln(F("Could not find port mapping in IGD"));
  }

  return isSuccess;
}

bool UPnP::deletePortMapping(GatewayInfo *deviceInfo, UpnpRule *rule_ptr) {
  if (!applyActionOnSpecificPortMapping(&SOAPActionDeletePortMapping,
                                        deviceInfo, rule_ptr)) {
    return false;
  }

  bool isSuccess = false;
  while (p_client->available()) {
    String line = p_client->readStringUntil('\r');
    debugPrint(line);
    if (line.indexOf(F("errorCode")) >= 0) {
      isSuccess = false;
      // flush response and exit loop
      while (p_client->available()) {
        line = p_client->readStringUntil('\r');
        debugPrint(line);
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
                                            GatewayInfo *deviceInfo,
                                            UpnpRule *rule_ptr) {
  UpnpRule &rule = *rule_ptr;
  char integer_string[32];
  debugPrint(F("Apply action ["));
  debugPrint(soapAction->name);
  debugPrint(F("] on port mapping ["));
  debugPrint(rule.devFriendlyName);
  debugPrintln(F("]"));

  // connect to IGD (TCP connection) again, if needed, in case we got
  // disconnected after the previous query
  unsigned long timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  if (!p_client->connected()) {
    while (!connectToIGD(deviceInfo->host, deviceInfo->actionPort)) {
      if (millis() > timeout) {
        debugPrintln(F("Timeout expired while trying to connect to the IGD"));
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
  strcat_P(body_tmp, deviceInfo->serviceTypeName.c_str());
  strcat_P(body_tmp,
           PSTR("\">\r\n<NewRemoteHost></NewRemoteHost>\r\n<NewExternalPort>"));
  sprintf(integer_string, "%d", rule_ptr->externalPort);
  strcat_P(body_tmp, integer_string);
  strcat_P(body_tmp, PSTR("</NewExternalPort>\r\n<NewProtocol>"));
  strcat_P(body_tmp, rule.protocol.c_str());
  strcat_P(body_tmp, PSTR("</NewProtocol>\r\n</u:"));
  strcat_P(body_tmp, soapAction->name);
  strcat_P(body_tmp, PSTR(">\r\n</s:Body>\r\n</s:Envelope>\r\n"));

  sprintf(integer_string, "%d", strlen(body_tmp));

  p_client->print(F("POST "));

  p_client->print(deviceInfo->actionPath);
  p_client->println(F(" HTTP/1.1"));
  p_client->println(F("Connection: close"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  p_client->println("Host: " + deviceInfo->host.toString() + ":" +
                    String(deviceInfo->actionPort));
  p_client->print(F("SOAPAction: \""));
  p_client->print(deviceInfo->serviceTypeName);
  p_client->print(F("#"));
  p_client->print(soapAction->name);
  p_client->println(F("\""));
  p_client->print(F("Content-Length: "));
  p_client->println(integer_string);
  p_client->println();

  p_client->println(body_tmp);
  p_client->println();

  debugPrintln(body_tmp);

  timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  while (p_client->available() == 0) {
    if (millis() > timeout) {
      debugPrintln(F("TCP connection timeout while retrieving port mappings"));
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
  if (p_udp->beginMulticast(WiFi.localIP(), ipMulti, 0)) {
    return true;
  }
#else
  if (p_udp->beginMulticast(ipMulti, UPNP_SSDP_PORT)) {
    return true;
  }
#endif

  debugPrintln(F("UDP connection failed"));
  return false;
}

// broadcast an M-SEARCH message to initiate messages from SSDP devices
// the router should respond to this message by a packet sent to this device's
// unicast addresss on the same UPnP port (1900)
void UPnP::broadcastMSearch(bool isSsdpAll /*=false*/) {
  char integer_string[32];

  debugPrint(F("Sending M-SEARCH to ["));
  debugPrint(ipMulti.toString());
  debugPrint(F("] Port ["));
  debugPrint(String(UPNP_SSDP_PORT));
  debugPrintln(F("]"));

#if defined(ESP8266)
  p_udp->beginPacketMulticast(ipMulti, UPNP_SSDP_PORT, WiFi.localIP());
#else
  uint8_t beginMulticastPacketRes = p_udp->beginPacket(ipMulti, UPNP_SSDP_PORT);
  debugPrint(F("beginMulticastPacketRes ["));
  debugPrint(String(beginMulticastPacketRes));
  debugPrintln(F("]"));
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

    debugPrintln(body_tmp);
    size_t len = strlen(body_tmp);
    debugPrint(F("M-SEARCH packet length is ["));
    debugPrint(String(len));
    debugPrintln(F("]"));

#if defined(ESP8266)
    p_udp->write(body_tmp);
#else
    p_udp->print(body_tmp);
#endif

    int endPacketRes = p_udp->endPacket();
    debugPrint(F("endPacketRes ["));
    debugPrint(String(endPacketRes));
    debugPrintln(F("]"));
  }

  debugPrintln(F("M-SEARCH packets sent"));
}

Vector<SsdpDevice> UPnP::listDevices() {
  devices.clear();
  if (timeoutMs <= 0) {
    debugPrintln(
        "Timeout must be set when initializing UPnP to use this method, "
        "exiting.");
    return devices;
  }

  unsigned long startTime = millis();
  while (!connectUDP()) {
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrint(F("Timeout expired while connecting UDP"));
      p_udp->stop();
      return devices;
    }
    delay(500);
    debugPrint(".");
  }
  debugPrintln("");  // \n

  broadcastMSearch(true);
  IPAddress gatewayIP = WiFi.gatewayIP();

  debugPrint(F("Gateway IP ["));
  debugPrint(gatewayIP.toString());
  debugPrintln(F("]"));

  SsdpDevice ssdpDevice;
  while (true) {
    ssdpDevice = waitForUnicastResponseToMSearch(
        ipNull);  // nullptr will cause finding all SSDP device (not just the
                  // IGD)
    if (timeoutMs > 0 && (millis() - startTime > timeoutMs)) {
      debugPrintln(
          F("Timeout expired while waiting for the gateway router to respond "
            "to M-SEARCH message"));
      p_udp->stop();
      break;
    }

    // ssdpDeviceToString(ssdpDevice_ptr);
    if (ssdpDevice) {
      if (!devices.contains(ssdpDevice))
        devices.push_back(ssdpDevice);
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
  if (gatewayIP != ipNull && remoteIP != gatewayIP) {
    debugPrint(F("Discarded packet not originating from IGD - gatewayIP ["));
    debugPrint(gatewayIP.toString());
    debugPrint(F("] remoteIP ["));
    debugPrint(remoteIP.toString());
    debugPrintln(F("]"));
    return newSsdpDevice;
  }

  debugPrint(F("Received packet of size ["));
  debugPrint(String(packetSize));
  debugPrint(F("]"));
  debugPrint(F(" ip ["));
  for (int i = 0; i < 4; i++) {
    debugPrint(String(remoteIP[i]));  // Decimal
    if (i < 3) {
      debugPrint(F("."));
    }
  }
  debugPrint(F("] port ["));
  debugPrint(String(p_udp->remotePort()));
  debugPrintln(F("]"));

  // sanity check
  if (packetSize > UPNP_UDP_TX_RESPONSE_MAX_SIZE) {
    debugPrint(
        F("Received packet with size larged than the response buffer, cannot "
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
    debugPrint(F("UDP packet read bytes ["));
    debugPrint(String(len));
    debugPrint(F("] out of ["));
    debugPrint(String(packetSize));
    debugPrintln(F("]"));
    memcpy(responseBuffer + idx, packetBuffer, len);
    idx += len;
  }
  responseBuffer[idx] = '\0';

  debugPrintln(F("Gateway packet content:"));
  debugPrintln(responseBuffer);

  const char *const *deviceList = deviceListUpnp;
  if (gatewayIP == ipNull) {
    deviceList = deviceListSsdpAll;
  }

  // only continue if the packet is a response to M-SEARCH and it originated
  // from a gateway device for SSDP discovery we continue anyway
  if (gatewayIP != ipNull) {  // for the use of listDevices
    bool foundIGD = false;
    for (int i = 0; deviceList[i]; i++) {
      if (strstr(responseBuffer, deviceList[i]) != nullptr) {
        foundIGD = true;
        debugPrint(F("IGD of type ["));
        debugPrint(deviceList[i]);
        debugPrintln(F("] found"));
        break;
      }
    }

    if (!foundIGD) {
      debugPrintln(F("IGD was not found"));
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
      debugPrintln(F("ERROR: could not extract value from LOCATION param"));
      return newSsdpDevice;
    }
  } else {
    debugPrintln(F("ERROR: LOCATION param was not found"));
    return newSsdpDevice;
  }

  debugPrint(F("Device location found ["));
  debugPrint(location);
  debugPrintln(F("]"));

  IPAddress host = getHost(location);
  int port = getPort(location);
  String path = getPath(location);

  newSsdpDevice.host = host;
  newSsdpDevice.port = port;
  newSsdpDevice.path = path;

  // debugPrintln(host.toString());
  // debugPrintln(String(port));
  // debugPrintln(path);

  return newSsdpDevice;
}

// a single trial to connect to the IGD (with TCP)
bool UPnP::connectToIGD(IPAddress host, int port) {
  debugPrint(F("Connecting to IGD with host ["));
  debugPrint(host.toString());
  debugPrint(F("] port ["));
  debugPrint(String(port));
  debugPrintln(F("]"));
  if (p_client->connect(host, port)) {
    debugPrintln(F("Connected to IGD"));
    return true;
  }
  return false;
}

// updates deviceInfo with the commands' information of the IGD
bool UPnP::getIGDEventURLs(GatewayInfo *deviceInfo) {
  debugPrintln(F("called getIGDEventURLs"));
  debugPrint(F("deviceInfo->actionPath ["));
  debugPrint(deviceInfo->actionPath);
  debugPrint(F("] deviceInfo->path ["));
  debugPrint(deviceInfo->path);
  debugPrintln(F("]"));

  // make an HTTP request
  p_client->print(F("GET "));
  p_client->print(deviceInfo->path);
  p_client->println(F(" HTTP/1.1"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  // p_client->println(F("Connection: close"));
  p_client->println("Host: " + deviceInfo->host.toString() + ":" +
                    String(deviceInfo->actionPort));
  p_client->println(F("Content-Length: 0"));
  p_client->println();

  // wait for the response
  unsigned long timeout = millis();
  while (p_client->available() == 0) {
    if (millis() - timeout > TCP_CONNECTION_TIMEOUT_MS) {
      debugPrintln(F("TCP connection timeout while executing getIGDEventURLs"));
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
    debugPrint(line);
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
        deviceInfo->actionPort = port;

        debugPrint(F("URLBase tag found ["));
        debugPrint(baseUrl);
        debugPrintln(F("]"));
        debugPrint(F("Translated to base host ["));
        debugPrint(host.toString());
        debugPrint(F("] and base port ["));
        debugPrint(String(port));
        debugPrintln(F("]"));
        urlBaseFound = true;
      }
    }

    // to support multiple <serviceType> tags
    int service_type_index_start = 0;

    for (int i = 0; deviceListUpnp[i]; i++) {
      int service_type_index =
          line.indexOf(String(UPNP_SERVICE_TYPE_TAG_START) + deviceListUpnp[i]);
      if (service_type_index >= 0) {
        debugPrint(F("["));
        debugPrint(deviceInfo->serviceTypeName);
        debugPrint(F("] service_type_index ["));
        debugPrint(String(service_type_index));
        debugPrintln("]");
        service_type_index_start = service_type_index;
        service_type_index =
            line.indexOf(UPNP_SERVICE_TYPE_TAG_END, service_type_index_start);
      }
      if (!upnpServiceFound && service_type_index >= 0) {
        index_in_line += service_type_index;
        upnpServiceFound = true;
        deviceInfo->serviceTypeName =
            getTagContent(line.substring(service_type_index_start),
                          UPNP_SERVICE_TYPE_TAG_NAME);
        debugPrint(F("["));
        debugPrint(deviceInfo->serviceTypeName);
        debugPrint(F("] service found! deviceType ["));
        debugPrint(deviceListUpnp[i]);
        debugPrintln(F("]"));
        break;  // will start looking for 'controlURL' now
      }
    }

    if (upnpServiceFound &&
        (index_in_line = line.indexOf("<controlURL>", index_in_line)) >= 0) {
      String controlURLContent =
          getTagContent(line.substring(index_in_line), "controlURL");
      if (controlURLContent.length() > 0) {
        deviceInfo->actionPath = controlURLContent;

        debugPrint(F("controlURL tag found! setting actionPath to ["));
        debugPrint(controlURLContent);
        debugPrintln(F("]"));

        // clear buffer
        debugPrintln(F("Flushing the rest of the response"));
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
bool UPnP::addPortMappingEntry(GatewayInfo *deviceInfo, UpnpRule *rule_ptr) {
  UpnpRule &rule = *rule_ptr;
  char integer_string[32];
  debugPrintln(F("called addPortMappingEntry"));

  // connect to IGD (TCP connection) again, if needed, in case we got
  // disconnected after the previous query
  unsigned long timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
  if (!p_client->connected()) {
    while (!connectToIGD(gatewayInfo.host, gatewayInfo.actionPort)) {
      if (millis() > timeout) {
        debugPrintln(F("Timeout expired while trying to connect to the IGD"));
        p_client->stop();
        return false;
      }
      delay(500);
    }
  }

  debugPrint(F("deviceInfo->actionPath ["));
  debugPrint(deviceInfo->actionPath);
  debugPrintln(F("]"));

  debugPrint(F("deviceInfo->serviceTypeName ["));
  debugPrint(deviceInfo->serviceTypeName);
  debugPrintln(F("]"));

  strcpy_P(body_tmp,
           PSTR("<?xml version=\"1.0\"?><s:Envelope "
                "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/"
                "\"><s:Body><u:AddPortMapping xmlns:u=\""));
  strcat_P(body_tmp, deviceInfo->serviceTypeName.c_str());
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
      (rule.internalAddr == ipNull) ? WiFi.localIP() : rule.internalAddr;
  strcat_P(body_tmp, ipAddress.toString().c_str());
  strcat_P(body_tmp, PSTR("</NewInternalClient><NewEnabled>1</"
                          "NewEnabled><NewPortMappingDescription>"));
  strcat_P(body_tmp, rule.devFriendlyName.c_str());
  strcat_P(body_tmp, PSTR("</NewPortMappingDescription><NewLeaseDuration>"));
  sprintf(integer_string, "%d", rule.leaseDurationSec);
  strcat_P(body_tmp, integer_string);
  strcat_P(
      body_tmp,
      PSTR("</NewLeaseDuration></u:AddPortMapping></s:Body></s:Envelope>"));

  sprintf(integer_string, "%d", strlen(body_tmp));

  p_client->print(F("POST "));
  p_client->print(deviceInfo->actionPath);
  p_client->println(F(" HTTP/1.1"));
  // p_client->println(F("Connection: close"));
  p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
  p_client->println("Host: " + deviceInfo->host.toString() + ":" +
                    String(deviceInfo->actionPort));
  // p_client->println(F("Accept: */*"));
  // p_client->println(F("Content-Type: application/x-www-form-urlencoded"));
  p_client->print(F("SOAPAction: \""));
  p_client->print(deviceInfo->serviceTypeName);
  p_client->println(F("#AddPortMapping\""));

  p_client->print(F("Content-Length: "));
  p_client->println(integer_string);
  p_client->println();

  p_client->println(body_tmp);
  p_client->println();

  debugPrint(F("Content-Length was: "));
  debugPrintln(integer_string);

  debugPrintln(body_tmp);

  timeout = millis();
  while (p_client->available() == 0) {
    if (millis() - timeout > TCP_CONNECTION_TIMEOUT_MS) {
      debugPrintln(F("TCP connection timeout while adding a port mapping"));
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
    debugPrintln(line);
  }
  debugPrintln("");  // \n

  if (!isSuccess) {
    p_client->stop();
  }

  return isSuccess;
}

bool UPnP::debugPortMappings() {
  char integer_string[32];
  // verify gateway information is valid
  // TODO: use this _gwInfo to skip the UDP part completely if it is not empty
  if (!gatewayInfo) {
    debugPrintln(F("Invalid router info, cannot continue"));
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
      while (!connectToIGD(gatewayInfo.host, gatewayInfo.actionPort)) {
        if (millis() > timeout) {
          debugPrint(F("Timeout expired while trying to connect to the IGD"));
          p_client->stop();
          ruleNodes.clear();
          return false;
        }
        delay(1000);
      }
    }

    debugPrint(F("Sending query for index ["));
    debugPrint(String(index));
    debugPrintln(F("]"));

    strcpy_P(
        body_tmp,
        PSTR(
            "<?xml version=\"1.0\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
            "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
            "<u:GetGenericPortMappingEntry xmlns:u=\""));
    strcat_P(body_tmp, gatewayInfo.serviceTypeName.c_str());
    strcat_P(body_tmp, PSTR("\">"
                            "  <NewPortMappingIndex>"));

    sprintf(integer_string, "%d", index);
    strcat_P(body_tmp, integer_string);
    strcat_P(body_tmp, PSTR("</NewPortMappingIndex>"
                            "</u:GetGenericPortMappingEntry>"
                            "</s:Body>"
                            "</s:Envelope>"));

    sprintf(integer_string, "%d", strlen(body_tmp));

    p_client->print(F("POST "));
    p_client->print(gatewayInfo.actionPath);
    p_client->println(F(" HTTP/1.1"));
    p_client->println(F("Connection: keep-alive"));
    p_client->println(F("Content-Type: text/xml; charset=\"utf-8\""));
    p_client->println("Host: " + gatewayInfo.host.toString() + ":" +
                      String(gatewayInfo.actionPort));
    p_client->print(F("SOAPAction: \""));
    p_client->print(gatewayInfo.serviceTypeName);
    p_client->println(F("#GetGenericPortMappingEntry\""));

    p_client->print(F("Content-Length: "));
    p_client->println(integer_string);
    p_client->println();

    p_client->println(body_tmp);
    p_client->println();

    timeout = millis() + TCP_CONNECTION_TIMEOUT_MS;
    while (p_client->available() == 0) {
      if (millis() > timeout) {
        debugPrintln(
            F("TCP connection timeout while retrieving port mappings"));
        p_client->stop();
        ruleNodes.clear();
        return false;
      }
    }

    while (p_client->available()) {
      String line = p_client->readStringUntil('\r');
      debugPrint(line);
      if (line.indexOf(PORT_MAPPING_INVALID_INDEX) >= 0) {
        reachedEnd = true;
      } else if (line.indexOf(PORT_MAPPING_INVALID_ACTION) >= 0) {
        debugPrint(F("Invalid action while reading port mappings"));
        reachedEnd = true;
      } else if (line.indexOf(F("HTTP/1.1 500 ")) >= 0) {
        debugPrint(
            F("Internal server error, likely because we have shown all the "
              "mappings"));
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

  // print nicely and free heap memory
  debugPrintln(F("IGD current port mappings:"));
  for (auto &rule : ruleNodes) {
    upnpRuleToString(&rule);
  }
  ruleNodes.clear();
  debugPrintln("");  // \n

  p_client->stop();

  return true;
}

void UPnP::debugConfig() {
  debugPrintln(F("UPnP configured port mappings:"));
  for (auto &rule : ruleNodes) {
    upnpRuleToString(&rule);
  }
  debugPrintln("");  // \n
}

// TODO: remove use of String
void UPnP::upnpRuleToString(UpnpRule *rule_ptr) {
  UpnpRule &rule = *rule_ptr;
  String index = String(rule.index);
  debugPrint(index);
  debugPrint(".");
  debugPrint(
      getSpacesString(5 - (index.length() + 1)));  // considering the '.' too

  String devFriendlyName = rule.devFriendlyName;
  debugPrint(devFriendlyName);
  debugPrint(getSpacesString(30 - devFriendlyName.length()));

  IPAddress ipAddress =
      (rule.internalAddr == ipNull) ? WiFi.localIP() : rule.internalAddr;
  String internalAddr = ipAddress.toString();
  debugPrint(internalAddr);
  debugPrint(getSpacesString(18 - internalAddr.length()));

  String internalPort = String(rule.internalPort);
  debugPrint(internalPort);
  debugPrint(getSpacesString(7 - internalPort.length()));

  String externalPort = String(rule.externalPort);
  debugPrint(externalPort);
  debugPrint(getSpacesString(7 - externalPort.length()));

  String protocol = rule.protocol;
  debugPrint(protocol);
  debugPrint(getSpacesString(7 - protocol.length()));

  String leaseDuration = String(rule.leaseDurationSec);
  debugPrint(leaseDuration);
  debugPrint(getSpacesString(7 - leaseDuration.length()));

  debugPrintln("");
}

void UPnP::debugDevices() {
  for (auto &device : listDevices()) {
    debugSsdpDevice(&device);
  }
}

void UPnP::debugSsdpDevice(SsdpDevice *ssdpDevice) {
  debugPrint(F("SSDP device ["));
  debugPrint(ssdpDevice->host.toString());
  debugPrint(F("] port ["));
  debugPrint(String(ssdpDevice->port));
  debugPrint(F("] path ["));
  debugPrint(ssdpDevice->path);
  debugPrintln(F("]"));
}

String UPnP::getSpacesString(int num) {
  if (num < 0) {
    num = 1;
  }
  String spaces = "";
  for (int i = 0; i < num; i++) {
    spaces += " ";
  }
  return spaces;
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
    debugPrint(F("ERROR: Cannot find path in url ["));
    debugPrint(url);
    debugPrintln(F("]"));
    return "";
  }
  return url.substring(firstSlashIndex, url.length());
}

String UPnP::getTagContent(const String &line, String tagName) {
  int startIndex = line.indexOf("<" + tagName + ">");
  if (startIndex == -1) {
    debugPrint(F("ERROR: Cannot find tag content in line ["));
    debugPrint(line);
    debugPrint(F("] for start tag [<"));
    debugPrint(tagName);
    debugPrintln(F(">]"));
    return "";
  }
  startIndex += tagName.length() + 2;
  int endIndex = line.indexOf("</" + tagName + ">", startIndex);
  if (endIndex == -1) {
    debugPrint(F("ERROR: Cannot find tag content in line ["));
    debugPrint(line);
    debugPrint(F("] for end tag [</"));
    debugPrint(tagName);
    debugPrintln(F(">]"));
    return "";
  }
  return line.substring(startIndex, endIndex);
}

}  // namespace tiny_dlna