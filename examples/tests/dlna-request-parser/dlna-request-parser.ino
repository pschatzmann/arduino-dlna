#include "DLNA.h"

RequestData data;
DLNARequestParser rp;

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaDebug);

  data.peer.address = IPAddress(1,2,3,4);
  data.peer.port = 5;
  data.data =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "MX: 10\r\n"
      "ST: urn:dial-multiscreen-org:service:dial:1\r\n"
      "USER-AGENT: Google Chrome/124.0.6367.201 Linux\r\n";

  MSearchReplySchedule* result = (MSearchReplySchedule*)rp.parse(data);

  assert(result!=nullptr);
  assert(result->getAddress().address == data.peer.address);
  assert(result->getAddress().port == data.peer.port);
  assert(result->mx == 10);
  assert(result->search_target.equals("urn:dial-multiscreen-org:service:dial:1"));
  Serial.println("OK");

}

void loop() {}