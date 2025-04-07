#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioTools/CoreAudio/Pipeline.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/CoreAudio/AudioHttp/URLStream.h"
#include "conmgr.h"
#include "control.h"
#include "dlna/DLNADeviceMgr.h"
#include "transport.h"

namespace tiny_dlna {

/**
 * @brief MediaRenderer DLNA Device
 * @author Phil Schatzmann
 */
class MediaRenderer : public DLNADevice {
 public:
  MediaRenderer() {
    // Constructor
    DlnaLogger.log(DlnaInfo, "MediaRenderer::MediaRenderer");
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("MediaRenderer");
    setManufacturer("TinyDLNA");
    setModelName("TinyDLNA");
    setModelNumber("1.0");
    setBaseURL("http://0.0.0.0:44757");
  }

  MediaRenderer(AudioStream& out, AudioDecoder& decoder) : MediaRenderer() {
    setOutput(out);
    setDecoder(decoder);
  }

  MediaRenderer(AudioOutput& out, AudioDecoder& decoder) : MediaRenderer() {
    setOutput(out);
    setDecoder(decoder);
  }

  MediaRenderer(Print& out, AudioDecoder& decoder) : MediaRenderer() {
    setOutput(out);
    setDecoder(decoder);
  }

  ~MediaRenderer() { end(); }

  /// Provides the optional login information
  void setLogin(const char* ssid, const char* password) {
    url.setPassword(password);
    url.setSSID(ssid);
  }

  bool begin() {
    if (!p_decoder) {
      DlnaLogger.log(DlnaError, "No decoder set");
      return false;
    }
    if (!p_stream && !p_out && !p_print) {
      DlnaLogger.log(DlnaError, "No output stream set");
      return false;
    }

    dec_stream.setDecoder(p_decoder);
    // Connect components
    // Set up audio pipeline
    if (pipeline.size() == 0) {
      pipeline.add(dec_stream);
      pipeline.add(volume);
    }
    // Add output stream
    if (p_stream) {
      pipeline.setOutput(*p_stream);
    } else if (p_out) {
      pipeline.setOutput(*p_out);
    } else if (p_print) {
      pipeline.setOutput(*p_print);
    } else {
      DlnaLogger.log(DlnaError, "No output stream set");
      return false;
    }
    bool result = pipeline.begin();

    return true;
  }

  void end() {
    // Stop playback and clean up
    stop();
    pipeline.end();
  }

  /// Defines and opens the URL to be played
  bool play(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaInfo, "Playing URL: %s", urlStr);

    // Stop any current playback
    stop();

    // Start network stream
    if (!url.begin(urlStr)) {
      DlnaLogger.log(DlnaError, "Failed to open URL");
      return false;
    }

    playing = true;
    start_time = millis();
    return true;
  }

  bool stop() {
    playing = false;
    return true;
  }

  bool pause() {
    if (!playing) return false;
    playing = false;
    return true;
  }

  bool resume() {
    playing = true;
    return true;
  }

  /// Defines the volume in percent (0-100)
  bool setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    float volumeFloat = volumePercent / 100.0;
    volume.setVolume(volumeFloat);
    current_volume = volumePercent;
    return true;
  }

  /// Returns the volume in percent (0-100)
  uint8_t getVolume() { return current_volume; }

  bool setMute(bool mute) {
    is_muted = mute;
    if (mute) {
      volume.setVolume(0);
    } else {
      volume.setVolume(current_volume / 100.0);
    }
    return true;
  }

  bool isMuted() { return is_muted; }

  bool isPlaying() { return playing; }

  bool isPaused() { return !playing; }

  bool seek(unsigned long position) {
    // Not fully supported with most streams
    DlnaLogger.log(DlnaWarning, "Seek not fully supported");
    return false;
  }

  unsigned long getPosition() {
    // Estimate position
    return millis() - start_time;
  }

  unsigned long getDuration() {
    return 0;  // Unknown for most streams
  }

  void setOutput(AudioStream& out) { p_stream = &out; }
  void setOutput(AudioOutput& out) { p_out = &out; }
  void setOutput(Print& out) { p_print = &out; }
  void setDecoder(AudioDecoder& decoder) { this->p_decoder = &decoder; }

  size_t copy() {
    // Call this in your main loop
    size_t bytes = 0;
    if (playing) {
      bytes = copier.copy();
    } else {
      delay(5);
    }
    return bytes;
  }

  /// loop called by DeviceMgr
  void loop() { copy(); }

  void setupServices(HttpServer& server, IUDPService& udp) {
    setupServicesImpl(&server);
  }

 protected:
  URLStream url;
  Pipeline pipeline;
  AudioDecoder* p_decoder = nullptr;
  EncodedAudioStream dec_stream;
  VolumeStream volume;
  AudioStream* p_stream = nullptr;
  AudioOutput* p_out = nullptr;
  Print* p_print = nullptr;
  StreamCopy copier{pipeline, url};
  uint8_t current_volume = 50;
  bool is_muted = false;
  bool playing = false;
  unsigned long start_time = 0;
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

  static const char* reply() {
    static const char* result =
        "<s:Envelope "
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/"
        "\"><s:Body><u:%1 "
        "xmlns:u=\"urn:schemas-upnp-org:service:%2:"
        "1\"></u:%1></s:Body></s:Envelope>";
    return result;
  }

  // Transport control handler
  static void transportControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Parse SOAP request and extract action
    Str soap = server->contentStr();
    DlnaLogger.log(DlnaInfo, "Transport Control: %s", soap.c_str());
    MediaRenderer& media_renderer = *((MediaRenderer*)hl->context[0]);

    Str reply_str{reply()};
    reply_str.replaceAll("%2", "AVTransport");
    if (soap.indexOf("Play") >= 0) {
      media_renderer.resume();
      reply_str.replaceAll("%1", "PlayResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Pause") >= 0) {
      media_renderer.pause();
      reply_str.replaceAll("%1", "PauseResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Stop") >= 0) {
      media_renderer.stop();
      reply_str.replaceAll("%1", "StopResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetAVTransportURI") >= 0) {
      // Extract URL from SOAP request
      int urlStart = soap.indexOf("<CurrentURI>") + 12;
      int urlEnd = soap.indexOf("</CurrentURI>");
      Str url = soap.substring(urlStart, urlEnd);
      media_renderer.play(url.c_str());
      reply_str.replaceAll("%1", "SetAVTransportURIResponse");
      server->reply("text/xml", reply_str.c_str());
    } else {
      // Handle other transport controls
      reply_str.replaceAll("%1", "ActionResponse");
      server->reply("text/xml", reply_str.c_str());
    }
  }

  // Rendering control handler
  static void renderingControlCB(HttpServer* server, const char* requestPath,
                                 HttpRequestHandlerLine* hl) {
    // Parse SOAP request and extract action
    MediaRenderer& media_renderer = *((MediaRenderer*)hl->context[0]);
    Str soap = server->contentStr();
    DlnaLogger.log(DlnaInfo, "Rendering Control: %s", soap.c_str());
    Str reply_str{reply()};
    reply_str.replaceAll("%2", "RenderingControl");

    if (soap.indexOf("SetVolume") >= 0) {
      // Extract volume from SOAP request
      int volStart = soap.indexOf("<DesiredVolume>") + 15;
      int volEnd = soap.indexOf("</DesiredVolume>");
      int volume = soap.substring(volStart, volEnd).toInt();
      media_renderer.setVolume(volume);
      reply_str.replaceAll("%1", "SetVolumeResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetMute") >= 0) {
      // Extract mute state from SOAP request
      int muteStart = soap.indexOf("<DesiredMute>") + 13;
      int muteEnd = soap.indexOf("</DesiredMute>");
      bool mute = (soap.substring(muteStart, muteEnd) == "1");
      media_renderer.setMute(mute);
      reply_str.replaceAll("%1", "SetMuteResponse");
      server->reply("text/xml", reply_str.c_str());
    } else {
      // Handle other rendering controls
      reply_str.replaceAll("%1", "RenderingControl");
      server->reply("text/xml", reply_str.c_str());
    }
  };

  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaInfo, "MediaRenderer::setupServices");

    auto transportCB = [](HttpServer* server, const char* requestPath,
                          HttpRequestHandlerLine* hl) {
      server->reply("text/xml", transport_xml);
    };

    auto connmgrCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", connmgr_xml);
    };

    auto controlCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", control_xml);
    };

    // define services
    DLNAServiceInfo rc, cm, avt;
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              transportCB, "/AVT/control", transportControlCB, "/AVT/event",
              [](HttpServer* server, const char* requestPath,
                 HttpRequestHandlerLine* hl) { server->replyOK(); });

    cm.setup(
        "urn:schemas-upnporg:service:ConnectionManager:1",
        "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
        connmgrCB, "/CM/control",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); },
        "/CM/event",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); });

    rc.setup("urn:schemas-upnporg:service:RenderingControl:1",
             "urn:upnp-org:serviceId:RenderingControl", "/RC/service.xml",
             controlCB, "/RC/control", renderingControlCB, "/RC/event",
             [](HttpServer* server, const char* requestPath,
                HttpRequestHandlerLine* hl) { server->replyOK(); });

    addService(rc);
    addService(cm);
    addService(avt);
  }
};

}  // namespace tiny_dlna
