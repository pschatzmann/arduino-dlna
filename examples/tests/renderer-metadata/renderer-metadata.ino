// Tests the handling of the DIDL-Lite metadata (CurrentURIMetaData) in the
// MediaRenderer: SOAP argument parsing, title/artist/album/mime extraction
// and the escaping of the metadata in the replies.
#include "dlna.h"

using namespace tiny_dlna;

// SetAVTransportURI request as sent by a control point for a MiniDLNA item
const char* set_uri_request =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>"
    "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<CurrentURI>http://192.168.1.10:8200/MediaItems/307.mp3</CurrentURI>"
    "<CurrentURIMetaData>&lt;DIDL-Lite "
    "xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/&quot; "
    "xmlns:dc=&quot;http://purl.org/dc/elements/1.1/&quot; "
    "xmlns:upnp=&quot;urn:schemas-upnp-org:metadata-1-0/upnp/&quot;&gt;"
    "&lt;item id=&quot;64$0$1&quot; parentID=&quot;64$0&quot; "
    "restricted=&quot;1&quot;&gt;"
    "&lt;dc:title&gt;My Song &amp;amp; &amp;quot;Friends&amp;quot;&lt;/dc:title&gt;"
    "&lt;upnp:artist&gt;The Artist&lt;/upnp:artist&gt;"
    "&lt;upnp:album&gt;The Album&lt;/upnp:album&gt;"
    "&lt;upnp:class&gt;object.item.audioItem.musicTrack&lt;/upnp:class&gt;"
    "&lt;res protocolInfo=&quot;http-get:*:audio/mpeg:*&quot;&gt;"
    "http://192.168.1.10:8200/MediaItems/307.mp3&lt;/res&gt;"
    "&lt;/item&gt;&lt;/DIDL-Lite&gt;</CurrentURIMetaData>"
    "</u:SetAVTransportURI></s:Body></s:Envelope>";

// Same action with dc:creator instead of upnp:artist and no album
const char* set_uri_request_creator =
    "<?xml version=\"1.0\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
    "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<CurrentURI>http://192.168.1.10:8200/MediaItems/308.flac</CurrentURI>"
    "<CurrentURIMetaData>&lt;DIDL-Lite&gt;&lt;item&gt;"
    "&lt;dc:title&gt;Second&lt;/dc:title&gt;"
    "&lt;dc:creator&gt;Creator&lt;/dc:creator&gt;"
    "&lt;res protocolInfo=&quot;http-get:*:audio/flac:*&quot;&gt;"
    "http://192.168.1.10:8200/MediaItems/308.flac&lt;/res&gt;"
    "&lt;/item&gt;&lt;/DIDL-Lite&gt;</CurrentURIMetaData>"
    "</u:SetAVTransportURI></s:Body></s:Envelope>";

// Same action without metadata
const char* set_uri_request_no_meta =
    "<?xml version=\"1.0\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
    "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "<CurrentURI>http://192.168.1.10:8200/MediaItems/309.mp3</CurrentURI>"
    "<CurrentURIMetaData></CurrentURIMetaData>"
    "</u:SetAVTransportURI></s:Body></s:Envelope>";

const char* get_media_info_request =
    "<?xml version=\"1.0\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
    "<u:GetMediaInfo xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
    "<InstanceID>0</InstanceID>"
    "</u:GetMediaInfo></s:Body></s:Envelope>";

const char* search_request =
    "<?xml version=\"1.0\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
    "<u:Search xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
    "<ContainerID>0</ContainerID>"
    "<SearchCriteria>upnp:class derivedfrom &quot;object.item.audioItem&quot; "
    "and dc:title contains &quot;A &amp; B&quot;</SearchCriteria>"
    "</u:Search></s:Body></s:Envelope>";

/// Client which provides the request body in chunks of max_chunk bytes
class MockClient : public Client {
 public:
  MockClient(const char* data, size_t maxChunk)
      : data(data), len(strlen(data)), max_chunk(maxChunk) {}
  int connect(IPAddress, uint16_t) override { return 1; }
  int connect(const char*, uint16_t) override { return 1; }
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t size) override { return size; }
  int available() override { return len - pos; }
  int read() override { return pos < len ? data[pos++] : -1; }
  int read(uint8_t* buf, size_t size) override {
    size_t n = min(min(size, max_chunk), len - pos);
    memcpy(buf, data + pos, n);
    pos += n;
    return n;
  }
  size_t readBytes(char* buf, size_t size) {
    return read((uint8_t*)buf, size);
  }
  int peek() override { return pos < len ? data[pos] : -1; }
  void flush() override {}
  void stop() override {}
  uint8_t connected() override { return pos < len; }
  operator bool() override { return true; }

 protected:
  const char* data;
  size_t len;
  size_t pos = 0;
  size_t max_chunk;
};

/// Print which collects the output in a Str
class StrOut : public Print {
 public:
  using Print::write;
  size_t write(uint8_t c) override {
    str.add((char)c);
    return 1;
  }
  Str str;
};

/// Client handler which provides the request and records the reply
class MockClientHandler : public IClientHandler {
 public:
  MockClientHandler(const char* body, size_t maxChunk)
      : mock_client(body, maxChunk) {
    request_header.put("Content-Length", (int)strlen(body));
  }
  void replyChunked(const char*, Stream&, int status, const char*) override {
    this->status = status;
  }
  void replyChunked(const char*, int status, const char*) override {
    this->status = status;
  }
  void reply(const char*, Stream&, int, int status, const char*) override {
    this->status = status;
  }
  void reply(const char*, size_t (*callback)(Print& out, void* ref),
             int status, const char*, void* ref) override {
    this->status = status;
    callback(out, ref);
  }
  void reply(const char*, const char* str, int status, const char*) override {
    this->status = status;
    out.print(str);
  }
  void reply(const char*, const uint8_t* str, int len, int status,
             const char*) override {
    this->status = status;
    out.write(str, len);
  }
  void replyOK() override { status = 200; }
  void replyNotFound() override { status = 404; }
  void replyError(int err, const char*) override { status = err; }
  void endClient() override {}
  void crlf() override {}
  Client* client() override { return &mock_client; }
  HttpRequestHeader& requestHeader() override { return request_header; }
  HttpReplyHeader& replyHeader() override { return reply_header; }

  int status = 0;
  StrOut out;

 protected:
  MockClient mock_client;
  HttpRequestHeader request_header;
  HttpReplyHeader reply_header;
};

/// Server which does nothing: the renderer just passes it along
class MockServer : public IHttpServer {
 public:
  IPAddress& localIP() override { return ip; }
  bool begin() override { return true; }
  void end() override {}
  void rewrite(const char*, const char*) override {}
  void on(const char*, TinyMethodID, web_callback_fn, void*[], int) override {}
  void on(const char*, TinyMethodID, const char*, web_callback_fn) override {}
  void on(const char*, TinyMethodID, const char*, const char*) override {}
  void on(const char*, TinyMethodID, const char*, const uint8_t*,
          int) override {}
  void on(const char*, TinyMethodID, Url&) override {}
  bool onRequest(const char*) override { return false; }
  void addHandler(HttpRequestHandlerLine*) override {}
  bool doLoop() override { return false; }
  bool copy() override { return false; }
  operator bool() override { return true; }
  bool isActive() override { return true; }
  const char* localHost() override { return "localhost"; }
  void setNoConnectDelay(int) override {}
  void setKeepAlive(bool) override {}
  void setReference(void* reference) override { ref = reference; }
  void* getReference() override { return ref; }

 protected:
  IPAddress ip{127, 0, 0, 1};
  void* ref = nullptr;
};

using Renderer = DLNAMediaRenderer<WiFiClient>;

/// Renderer which gives access to the action processing
class TestRenderer : public Renderer {
 public:
  using Renderer::processAction;
};

MockServer server;
TestRenderer renderer;
int set_uri_events = 0;
Str title_in_event;

void handleEvent(MediaEvent ev, Renderer& mr) {
  if (ev == MediaEvent::SET_URI) {
    set_uri_events++;
    title_in_event = mr.getTitle();
  }
}

/// Parses the request like the http server would and returns the action
void parse(const char* body, size_t maxChunk, ActionRequest& action) {
  MockClientHandler client(body, maxChunk);
  Renderer::DeviceType::parseActionRequest(&server, client, "/control",
                                           nullptr, action);
}

/// Parses the request and lets the renderer process it
int process(const char* body, size_t maxChunk, Str* reply = nullptr) {
  ActionRequest action;
  MockClientHandler client(body, maxChunk);
  Renderer::DeviceType::parseActionRequest(&server, client, "/control",
                                           nullptr, action);
  renderer.processAction(action, server, client);
  if (reply) *reply = client.out.str;
  return client.status;
}

void testUnEscape() {
  Str str("&lt;a b=&quot;1&quot; c=&apos;2&apos;&gt;x &amp;amp; y&lt;/a&gt;");
  XMLAttributeParser::unEscape(str);
  assert(str.equals("<a b=\"1\" c='2'>x &amp; y</a>"));
  Serial.println("testUnEscape OK");
}

void testExtractElementText() {
  const char* xml =
      "<item><dc:titleX>wrong</dc:titleX><dc:title id=\"1\">A &amp; B"
      "</dc:title><upnp:album/></item>";
  Str out;
  assert(XMLAttributeParser::extractElementText(xml, "dc:title", out));
  assert(out.equals("A & B"));
  assert(!XMLAttributeParser::extractElementText(xml, "upnp:artist", out));
  assert(out.isEmpty());
  assert(!XMLAttributeParser::extractElementText(xml, "upnp:album", out));
  assert(!XMLAttributeParser::extractElementText(nullptr, "dc:title", out));
  Serial.println("testExtractElementText OK");
}

void testParseArguments(size_t maxChunk) {
  ActionRequest action;
  parse(set_uri_request, maxChunk, action);
  assert(StrView(action.getAction()).endsWith("SetAVTransportURI"));
  assert(StrView(action.getArgumentValue("CurrentURI"))
             .equals("http://192.168.1.10:8200/MediaItems/307.mp3"));
  StrView meta(action.getArgumentValue("CurrentURIMetaData"));
  assert(meta.startsWith("<DIDL-Lite xmlns=\""));
  assert(meta.contains("<dc:title>My Song &amp; &quot;Friends&quot;</dc:title>"));
  assert(meta.endsWith("</DIDL-Lite>"));
  // the DIDL content must not show up as separate arguments
  assert(StrView(action.getArgumentValue("dc:title")).isEmpty());

  ActionRequest search;
  parse(search_request, maxChunk, search);
  assert(StrView(search.getArgumentValue("SearchCriteria"))
             .equals("upnp:class derivedfrom \"object.item.audioItem\" and "
                     "dc:title contains \"A & B\""));
  Serial.print("testParseArguments OK - chunk size ");
  Serial.println(maxChunk);
}

void testSetAVTransportURI(size_t maxChunk) {
  set_uri_events = 0;
  title_in_event.clear();
  assert(process(set_uri_request, maxChunk) == 200);
  assert(StrView(renderer.getCurrentUri())
             .equals("http://192.168.1.10:8200/MediaItems/307.mp3"));
  assert(StrView(renderer.getCurrentUriMetadata()).startsWith("<DIDL-Lite"));
  assert(StrView(renderer.getTitle()).equals("My Song & \"Friends\""));
  assert(StrView(renderer.getArtist()).equals("The Artist"));
  assert(StrView(renderer.getAlbum()).equals("The Album"));
  assert(StrView(renderer.getMime()).equals("audio/mpeg"));
  // title is available when the application gets the SET_URI event
  assert(set_uri_events == 1);
  assert(title_in_event.equals("My Song & \"Friends\""));

  // dc:creator is used if there is no upnp:artist; no stale album
  assert(process(set_uri_request_creator, maxChunk) == 200);
  assert(StrView(renderer.getTitle()).equals("Second"));
  assert(StrView(renderer.getArtist()).equals("Creator"));
  assert(StrView(renderer.getAlbum()).isEmpty());
  assert(StrView(renderer.getMime()).equals("audio/flac"));

  // no metadata: previous values are cleared
  assert(process(set_uri_request_no_meta, maxChunk) == 200);
  assert(StrView(renderer.getCurrentUri())
             .equals("http://192.168.1.10:8200/MediaItems/309.mp3"));
  assert(StrView(renderer.getCurrentUriMetadata()).isEmpty());
  assert(StrView(renderer.getTitle()).isEmpty());
  assert(StrView(renderer.getArtist()).isEmpty());
  assert(renderer.getMime() == nullptr);
  Serial.print("testSetAVTransportURI OK - chunk size ");
  Serial.println(maxChunk);
}

void testGetMediaInfoReply() {
  assert(process(set_uri_request, 1024) == 200);
  Str reply;
  assert(process(get_media_info_request, 1024, &reply) == 200);
  // the metadata must be escaped in the reply ...
  assert(reply.contains("<CurrentURIMetaData>&lt;DIDL-Lite"));
  assert(!reply.contains("<DIDL-Lite"));
  // ... so that a control point gets back the original metadata
  ActionRequest parsed;
  parse(reply.c_str(), 1024, parsed);
  assert(StrView(parsed.getArgumentValue("CurrentURIMetaData"))
             .equals(renderer.getCurrentUriMetadata()));
  Serial.println("testGetMediaInfoReply OK");
}

void setup() {
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Warning);
  renderer.setMediaEventHandler(handleEvent);
  server.setReference(&renderer.device());

  testUnEscape();
  testExtractElementText();
  for (size_t chunk : {1024, 64, 7}) {
    testParseArguments(chunk);
    testSetAVTransportURI(chunk);
  }
  testGetMediaInfoReply();
  Serial.println("All tests OK");
  exit(0);
}

void loop() {}
