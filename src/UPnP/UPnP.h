
/*
 * TinyUPnP.h - Library for creating UPnP rules automatically in your router.
 * Created by Ofek Pearl, September 2017.
 * Released into the public domain.
 */

#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <limits.h>

#ifdef UPNP_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#else
#define debugPrint(...)
#define debugPrintln(...)
#endif

// #define UPNP_DEBUG // uncomment to enable debug and TinyUPnP::print<...>()
// outputs
#define UPNP_SSDP_PORT 1900
#define TCP_CONNECTION_TIMEOUT_MS 6000
// after 6 tries of begin we will execute the more extensive addPortMapping
#define MAX_NUM_OF_UPDATES_WITH_NO_EFFECT 6
// reduce max UDP packet size to conserve memory (by default
// UDP_TX_PACKET_MAX_SIZE=8192)
#define UPNP_UDP_TX_PACKET_MAX_SIZE 1000
#define UPNP_UDP_TX_RESPONSE_MAX_SIZE 8192
#define MAX_TMP_SIZE 1200

namespace tiny_dlna {

// indication to update rules when the IP of the device changes
IPAddress ipNull(0, 0, 0, 0);

typedef void (*callback_function)(void);

/**
 * @brief UPnP Device information
 * @internal
 */

struct SsdpDevice {
  IPAddress host;
  int port;  // this port is used when getting router capabilities and xml files
  String path;  // this is the path that is used to retrieve router information
                // from xml files
};

/**
 * @brief UPnP Device Node
 * @internal
 */

struct SsdpDeviceNode {
  SsdpDevice *ssdpDevice;
  SsdpDeviceNode *next;
};

/**
 * @brief Basic UPnP Discovery Functionality
 */
class UPnP {
  friend class DLNADevice;

 public:
  /// timeoutMs - timeout in milli seconds for the operations of this class, 0
  /// for blocking operation
  UPnP(unsigned long timeoutMs = 20000) {
    timeoutMs = timeoutMs;
    headRuleNode = nullptr;
  }

  UPnP(Client &client, UDP &udp, unsigned long timeoutMs = 20000) {
    timeoutMs = timeoutMs;
    headRuleNode = nullptr;
    p_udp = &udp;
    p_client = &client;
  }

  // when the ruleIP is set to the current device IP, the IP of the rule will
  // change if the device changes its IP this makes sure the traffic will be
  // directed to the device even if the IP chnages
  void addConfig(int rulePort, const char *ruleProtocol,
                 int ruleLeaseDurationSec, const char *ruleFriendlyName) {
    addConfig(ipNull, rulePort, rulePort, ruleProtocol, ruleLeaseDurationSec,
              ruleFriendlyName);
  }

  void addConfig(IPAddress ruleIP, int rulePort, const char *ruleProtocol,
                 int ruleLeaseDurationSec, const char *ruleFriendlyName) {
    addConfig(ruleIP, rulePort, rulePort, ruleProtocol, ruleLeaseDurationSec,
              ruleFriendlyName);
  }

  void addConfig(IPAddress ruleIP, int ruleInternalPort, int ruleExternalPort,
                 const char *ruleProtocol, int ruleLeaseDurationSec,
                 const char *ruleFriendlyName);

  /// Discovers the devices on the network
  bool begin();
  /// Rediscovers the devices on the network
  bool update(unsigned long intervalMs,
              callback_function fallback = nullptr /* optional */);

  void end() {
    p_client->stop();
    p_udp->stop();
    isActive = false;
  }

  /// checks access to the network
  bool checkConnectivity(unsigned long startTime = 0);
  /// will create an object with all SSDP devices on the network
  SsdpDeviceNode *listDevices();
  // will print all SSDP devices
  void debugDevices(SsdpDeviceNode *SsdpDeviceNode);
  // prints all the port mappings that werea added using `addConfig`
  void debugConfig();
  /// Retrievs and prints the port mappings from the network
  bool debugPortMappings();

  void beginPostAlive(int intervallSec);
  void postAlive();

  char *getTempBuffer() { return body_tmp; }

  UDP *getUDP() { return p_udp; }

 protected:
  /**
   * @brief name of UPnP SOAP Action
   * @internal
   */
  struct SOAPAction {
    SOAPAction(const char *n) { name = n; }
    const char *name;
  };

  /**
   * @brief UPnP Gateway Information
   * @internal
   */
  struct GatewayInfo {
    GatewayInfo() { clear(); }
    // router info
    IPAddress host;
    int port;     // this port is used when getting router capabilities and xml
                  // files
    String path;  // this is the path that is used to retrieve router
                  // information from xml files

    // info for actions
    int actionPort;     // this port is used when performing SOAP API actions
    String actionPath;  // this is the path used to perform SOAP API actions
    String serviceTypeName;  // i.e "WANPPPConnection:1" or "WANIPConnection:1"

    void clear() {
      host = IPAddress(0, 0, 0, 0);
      port = 0;
      path = "";
      actionPort = 0;
      actionPath = "";
      serviceTypeName = "";
    }

    bool isValid() {
      debugPrint(F("isGatewayInfoValid ["));
      debugPrint(host.toString());
      debugPrint(F("] port ["));
      debugPrint(String(port));
      debugPrint(F("] path ["));
      debugPrint(path);
      debugPrint(F("] actionPort ["));
      debugPrint(String(actionPort));
      debugPrint(F("] actionPath ["));
      debugPrint(actionPath);
      debugPrint(F("] serviceTypeName ["));
      debugPrint(serviceTypeName);
      debugPrintln(F("]"));

      if (host == IPAddress(0, 0, 0, 0) || port == 0 || path.length() == 0 ||
          actionPort == 0) {
        debugPrintln(F("Gateway info is not valid"));
        return false;
      }

      debugPrintln(F("Gateway info is valid"));
      return true;
    }
  };

  /**
   * @brief UPnP Rule
   * @internal
   */
  struct UpnpRule {
    int index;
    String devFriendlyName;
    IPAddress internalAddr;
    int internalPort;
    int externalPort;
    String protocol;
    int leaseDurationSec;
  };

  /**
   * @brief UPnP Rule Node information
   * @internal
   */

  struct UpnpRuleNode {
    UpnpRule *upnpRule;
    UpnpRuleNode *next;
  };

  /**
   * @brief PortMappingResult enum
   * @internal
   */
  enum class PortMappingResult {
    UNKNOWN,
    SUCCESS,         // port mapping was added
    ALREADY_MAPPED,  // the port mapping is already found in the IGD
    EMPTY_PORT_MAPPING_CONFIG,
    NETWORK_ERROR,
    TIMEOUT,
    VERIFICATION_FAILED,
    NOP  // the check is delayed
  };

  UpnpRuleNode *headRuleNode = nullptr;
  unsigned long lastUpdateTime = 0;
  long timeoutMs;  // 0 for blocking operation
  WiFiUDP udpClient;
  UDP *p_udp = &udpClient;
  WiFiClient wifiClient;
  Client *p_client = &wifiClient;
  GatewayInfo gatewayInfo;
  unsigned long consequtiveFails = 0;
  PortMappingResult lastResult = PortMappingResult::UNKNOWN;
  bool isActive = false;
  // buffer to hold incoming packet
  char packetBuffer[UPNP_UDP_TX_PACKET_MAX_SIZE];
  char responseBuffer[UPNP_UDP_TX_RESPONSE_MAX_SIZE];
  char body_tmp[MAX_TMP_SIZE];
  const char *UPNP_SERVICE_TYPE_TAG_NAME = "serviceType";
  const char *UPNP_SERVICE_TYPE_TAG_START = "<serviceType>";
  const char *UPNP_SERVICE_TYPE_TAG_END = "</serviceType>";
  const char *PORT_MAPPING_INVALID_INDEX =
      "<errorDescription>SpecifiedArrayIndexInvalid</errorDescription>";
  const char *PORT_MAPPING_INVALID_ACTION =
      "<errorDescription>Invalid Action</errorDescription>";
  const char *const deviceListUpnp[6] = {
      "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
      "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
      "urn:schemas-upnp-org:service:WANIPConnection:1",
      "urn:schemas-upnp-org:service:WANIPConnection:2",
      "urn:schemas-upnp-org:service:WANPPPConnection:1",
      // "upnp:rootdevice",
      0};

  const char *const deviceListSsdpAll[2] = {"ssdp:all", 0};
  IPAddress ipMulti{239, 255, 255, 250};           // multicast address for SSDP
  IPAddress connectivityTestIp{64, 233, 187, 99};  // Google
  SOAPAction SOAPActionGetSpecificPortMappingEntry{
      "GetSpecificPortMappingEntry"};
  SOAPAction SOAPActionDeletePortMapping{"DeletePortMapping"};
  int postEveryMs = 3000;

  bool connectUDP();
  void broadcastMSearch(bool isSsdpAll = false);
  SsdpDevice *waitForUnicastResponseToMSearch(IPAddress gatewayIP);
  bool getGatewayInfo(GatewayInfo *deviceInfo, long startTime);
  bool connectToIGD(IPAddress host, int port);
  bool getIGDEventURLs(GatewayInfo *deviceInfo);
  bool addPortMappingEntry(GatewayInfo *deviceInfo, UpnpRule *rule_ptr);
  bool verifyPortMapping(GatewayInfo *deviceInfo, UpnpRule *rule_ptr);
  bool deletePortMapping(GatewayInfo *deviceInfo, UpnpRule *rule_ptr);
  bool applyActionOnSpecificPortMapping(SOAPAction *soapAction,
                                        GatewayInfo *deviceInfo,
                                        UpnpRule *rule_ptr);
  void removeAllPortMappingsFromIGD();
  // char* ipAddressToCharArr(IPAddress ipAddress);  // ?? not sure this is
  // needed
  void upnpRuleToString(UpnpRule *rule_ptr);
  String getSpacesString(int num);
  IPAddress getHost(String url);
  int getPort(String url);
  String getPath(String url);
  String getTagContent(const String &line, String tagName);
  void ssdpDeviceToString(SsdpDevice *ssdpDevice);
};

}  // namespace tiny_dlna