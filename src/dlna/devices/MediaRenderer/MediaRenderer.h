#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/CoreAudio/StreamCopy.h"
#include "AudioTools/CoreAudio/Pipeline.h"
#include "AudioTools/CoreAudio/VolumeStream.h"
#include "AudioTools/Communication/HTTP/URLStream.h"
#include "mr_conmgr.h"
#include "mr_control.h"
#include "mr_transport.h"
#include "dlna/DLNADeviceMgr.h"

namespace tiny_dlna {

/**
 * @brief MediaRenderer DLNA Device
 *
 * MediaRenderer implements a simple UPnP/DLNA Media Renderer device. It
 * provides functionality to receive a stream URL via UPnP AVTransport actions
 * and play it through an audio pipeline composed of a decoder, volume
 * control and an output stream. The class also exposes basic rendering
 * control actions such as volume, mute and transport controls (play/pause/stop).
 *
 * Usage summary:
 * - Configure an audio output and a decoder using setOutput() and
 *   setDecoder().
 * - Optionally provide WiFi credentials using setLogin().
 * - Call begin() to initialize the audio pipeline before playback.
 * - Use play(url) to start playback of a network stream (URLStream).
 * - Call loop() or copy() periodically from the main loop to process audio.
 *
 * This class intentionally keeps the implementation small and Arduino
 * friendly: methods return bool for success/failure and avoid dynamic memory
 * allocations in the hot path.
 *
 * Author: Phil Schatzmann
 */
class MediaRenderer : public DLNADevice {
 public:
  /**
   * @brief Default constructor
   *
   * Initializes device metadata (friendly name, manufacturer, model) and
   * sets default base URL and identifiers. It does not configure any audio
   * pipeline components; use setOutput() and setDecoder() for that.
   */
  MediaRenderer() {
    // Constructor
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::MediaRenderer");
    setSerialNumber(usn);
    setDeviceType(st);
    setFriendlyName("MediaRenderer");
    setManufacturer("TinyDLNA");
    setModelName("TinyDLNA");
    setModelNumber("1.0");
    setBaseURL("http://localhost:44757");
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

  /// Provide optional login information (SSID, password)
  void setLogin(const char* ssid, const char* password) {
    url.setPassword(password);
    url.setSSID(ssid);
  }

  /// Initializes the audio pipeline; must be called before playback
  bool begin() override {
    if (!p_decoder) {
      DlnaLogger.log(DlnaLogLevel::Error, "No decoder set");
      return false;
    }
    if (!p_stream && !p_out && !p_print) {
      DlnaLogger.log(DlnaLogLevel::Error, "No output stream set");
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
      DlnaLogger.log(DlnaLogLevel::Error, "No output stream set");
      return false;
    }
    bool result = pipeline.begin();
    if (!result) {
      DlnaLogger.log(DlnaLogLevel::Error, "Pipeline begin failed");
      return false;
    }

    return true;
  }

  /// ends the renderer
  void end() {
    // Stop playback and clean up
    stop();
    pipeline.end();
  }

  /// Start playback of a network resource (returns true on success)
  bool play(const char* urlStr) {
    if (urlStr == nullptr) return false;
    DlnaLogger.log(DlnaLogLevel::Info, "is_active URL: %s", urlStr);

    // Stop any current playback
    stop();

    // Start network stream
    if (!url.begin(urlStr)) {
      DlnaLogger.log(DlnaLogLevel::Error, "Failed to open URL");
      return false;
    }

    is_active = true;
    start_time = millis();
    return true;
  }

  /// Set the renderer volume (0..100 percent)
  bool setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    float volumeFloat = volumePercent / 100.0;
    volume.setVolume(volumeFloat);
    current_volume = volumePercent;
    return true;
  }

  /// Get current volume (0..100 percent)
  uint8_t getVolume() { return current_volume; }

  /// Enable or disable mute
  bool setMute(bool mute) {
    is_muted = mute;
    if (mute) {
      volume.setVolume(0);
    } else {
      volume.setVolume(current_volume / 100.0);
    }
    return true;
  }

  /// Query mute state
  bool isMuted() { return is_muted; }

  /// Query whether renderer is active (playing or ready)
  bool isActive() { return is_active; }

  /// Set the active state (used by transport callbacks)
  void setActive(bool active) { is_active = active; }

  /// Seek to a position (ms) — typically unsupported and returns false
  bool seek(unsigned long position) {
    // Not fully supported with most streams
    DlnaLogger.log(DlnaLogLevel::Warning, "Seek not fully supported");
    return false;
  }

  /// Get estimated playback position (ms)
  unsigned long getPosition() {
    // Estimate position; return 0 if not active or start_time not set
    if (!is_active || start_time == 0) return 0;
    return millis() - start_time;
  }

  /// Get duration of the current media (returns 0 when unknown)
  unsigned long getDuration() {
    return 0;  // Unknown for most streams
  }

  /// Output and decoder configuration methods
  void setOutput(AudioStream& out) { p_stream = &out; }
  void setOutput(AudioOutput& out) { p_out = &out; }
  void setOutput(Print& out) { p_print = &out; }
  void setDecoder(AudioDecoder& decoder) { this->p_decoder = &decoder; }
  

  /// Process pending audio data; call regularly from main loop
  size_t copy() {
    // Call this in your main loop
    size_t bytes = 0;
    if (is_active) {
      bytes = copier.copy();
    } else {
      delay(5);
    }
    return bytes;
  }

  /// loop called by DeviceMgr: just calls copy()
  /// Per-device loop (delegates to copy())
  void loop() { copy(); }


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
  bool is_active = false;
  unsigned long start_time = 0;
  const char* st = "urn:schemas-upnp-org:device:MediaRenderer:1";
  const char* usn = "uuid:09349455-2941-4cf7-9847-1dd5ab210e97";

  void setupServices(HttpServer& server, IUDPService& udp) {
    setupServicesImpl(&server);
  }

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
    DlnaLogger.log(DlnaLogLevel::Info, "Transport Control: %s", soap.c_str());
    MediaRenderer& media_renderer = *((MediaRenderer*)hl->context[0]);

    Str reply_str{reply()};
    reply_str.replaceAll("%2", "AVTransport");
    if (soap.indexOf("Play") >= 0) {
      media_renderer.setActive(true);
      reply_str.replaceAll("%1", "PlayResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Pause") >= 0) {
      media_renderer.setActive(false);
      reply_str.replaceAll("%1", "PauseResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("Stop") >= 0) {
      media_renderer.setActive(false);
      reply_str.replaceAll("%1", "StopResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetAVTransportURI") >= 0) {
      // Extract URL from SOAP request safely
      int urlTagStart = soap.indexOf("<CurrentURI>");
      int urlTagEnd = soap.indexOf("</CurrentURI>");
      if (urlTagStart >= 0 && urlTagEnd > urlTagStart) {
        int urlStart = urlTagStart + (int)strlen("<CurrentURI>");
        Str url = soap.substring(urlStart, urlTagEnd);
        media_renderer.play(url.c_str());
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetAVTransportURI called with invalid SOAP payload");
      }
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
    DlnaLogger.log(DlnaLogLevel::Info, "Rendering Control: %s", soap.c_str());
    Str reply_str{reply()};
    reply_str.replaceAll("%2", "RenderingControl");

    if (soap.indexOf("SetVolume") >= 0) {
      // Extract volume from SOAP request safely
      int volTagStart = soap.indexOf("<DesiredVolume>");
      int volTagEnd = soap.indexOf("</DesiredVolume>");
      if (volTagStart >= 0 && volTagEnd > volTagStart) {
        int volStart = volTagStart + (int)strlen("<DesiredVolume>");
        int volume = soap.substring(volStart, volTagEnd).toInt();
        media_renderer.setVolume(volume);
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetVolume called with invalid SOAP payload");
      }
      reply_str.replaceAll("%1", "SetVolumeResponse");
      server->reply("text/xml", reply_str.c_str());
    } else if (soap.indexOf("SetMute") >= 0) {
      // Extract mute state from SOAP request safely
      int muteTagStart = soap.indexOf("<DesiredMute>");
      int muteTagEnd = soap.indexOf("</DesiredMute>");
      if (muteTagStart >= 0 && muteTagEnd > muteTagStart) {
        int muteStart = muteTagStart + (int)strlen("<DesiredMute>");
        bool mute = (soap.substring(muteStart, muteTagEnd) == "1");
        media_renderer.setMute(mute);
      } else {
        DlnaLogger.log(DlnaLogLevel::Warning,
                       "SetMute called with invalid SOAP payload");
      }
      reply_str.replaceAll("%1", "SetMuteResponse");
      server->reply("text/xml", reply_str.c_str());
    } else {
      // Handle other rendering controls
      reply_str.replaceAll("%1", "RenderingControl");
      server->reply("text/xml", reply_str.c_str());
    }
  };

  void setupServicesImpl(HttpServer* server) {
    DlnaLogger.log(DlnaLogLevel::Info, "MediaRenderer::setupServices");

    auto transportCB = [](HttpServer* server, const char* requestPath,
                          HttpRequestHandlerLine* hl) {
      server->reply("text/xml", mr_connmgr_xml);
    };

    auto connmgrCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", mr_connmgr_xml);
    };

    auto controlCB = [](HttpServer* server, const char* requestPath,
                        HttpRequestHandlerLine* hl) {
      server->reply("text/xml", mr_control_xml);
    };

    // define services
    DLNAServiceInfo rc, cm, avt;
    avt.setup("urn:schemas-upnp-org:service:AVTransport:1",
              "urn:upnp-org:serviceId:AVTransport", "/AVT/service.xml",
              transportCB, "/AVT/control", transportControlCB, "/AVT/event",
              [](HttpServer* server, const char* requestPath,
                 HttpRequestHandlerLine* hl) { server->replyOK(); });

  cm.setup(
    "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:upnp-org:serviceId:ConnectionManager", "/CM/service.xml",
        connmgrCB, "/CM/control",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); },
        "/CM/event",
        [](HttpServer* server, const char* requestPath,
           HttpRequestHandlerLine* hl) { server->replyOK(); });

  rc.setup("urn:schemas-upnp-org:service:RenderingControl:1",
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
