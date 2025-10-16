
This directory contains implementations of DLNA devices

# DLNA Devices

This directory contains implementations of DLNA devices used by the
library (for example `MediaRenderer`).

## Creating a new DLNA device

This guide explains how to add a new DLNA device to the library. The
`MediaRenderer` device in this folder is a good example: it implements a
minimal UPnP/DLNA Media Renderer that receives an AVTransport URI and plays
audio through the Arduino-friendly AudioTools pipeline.

Follow these steps to add your own device:

1. Create a new header file for your device in this folder, for example
   `MyDevice.h` (this library is header-only; do not add `.cpp` files).

2. Subclass `DLNADevice` in the `tiny_dlna` namespace and implement any
	device-specific behaviour. See `MediaRenderer` for a compact example.

3. Provide device metadata in the constructor (device type, USN, friendly
	name, model, manufacturer, base URL, etc.). Use the provided helper
	methods from `DLNADevice`:

	- `setSerialNumber(const char* sn)`
	- `setDeviceType(const char* type)`
	- `setFriendlyName(const char* name)`
	- `setManufacturer(const char* mfr)`
	- `setModelName(const char* model)`
	- `setModelNumber(const char* number)`
	- `setBaseURL(const char* url)`

4. Expose any configuration methods needed by your device consumers. For
	example, `MediaRenderer` provides `setOutput(...)`, `setDecoder(...)` and
	`setLogin(...)` so the application can plug the audio pipeline into the
	device.

5. Implement `begin()` and `end()` to initialise and tear down hardware
	resources or internal pipelines. `begin()` should return `true` on
	success. `end()` should stop playback and clean up resources.

6. Implement a `loop()` method (or `copy()` for work-per-call) that is
	called regularly by the device manager. Keep these methods non-blocking
	where possible; short delays (e.g. `delay(5)`) are acceptable for idle
	waits on microcontrollers.

7. Provide UPnP service descriptions and handlers. Devices typically need
	to expose one or more services (AVTransport, RenderingControl,
	ConnectionManager). `MediaRenderer` shows a small helper `setupServicesImpl`
	that registers service XML and control/event endpoints using
	`DLNAServiceInfo` and `addService()`.

8. Implement SOAP control callbacks for each service. The pattern used in
	`MediaRenderer` is:

	- Provide a static callback function for transport control (e.g.
	  `transportControlCB`) and rendering control (`renderingControlCB`).
	- Parse the incoming SOAP (available through `server->contentStr()`),
	  detect the action (Play/Pause/Stop/SetAVTransportURI/SetVolume/SetMute)
	  and perform the corresponding device operation.
	- Return a simple SOAP reply using the `server->reply(...)` helper.

9. Add your device to the `DLNADeviceMgr` in your application code and
	start the manager. Example (Arduino sketch):

```cpp
#include <DLNADeviceMgr.h>
#include "MediaRenderer/MediaRenderer.h"

using namespace tiny_dlna;

MediaRenderer renderer;

void setup() {
  // Configure audio output/decoder as required by your platform
  // renderer.setOutput(myAudioStream);
  // renderer.setDecoder(myDecoder);

  // Start device (initialises pipeline, registers services)
  renderer.begin();

  // Register device with manager (assumes a global manager exists)
  DLNADeviceMgr::instance().addDevice(renderer);
  DLNADeviceMgr::instance().begin();
}

void loop() {
  // Regularly call the device manager loop which in turn calls device loops
  DLNADeviceMgr::instance().loop();
}
```

10. Test with a control point. You can use standard UPnP control point
	 applications (e.g., VLC, BubbleUPnP on Android) to discover and control
	 the device. Use the control point to send `SetAVTransportURI` then
	 `Play` and verify audio plays through the configured output.

### Notes and recommendations

- Keep network and I/O operations asynchronous where possible to avoid
  blocking the main loop for long periods.
- Follow `MediaRenderer`'s example for lightweight and Arduino-friendly
  implementations: avoid dynamic memory allocation in the hot path and use
  simple return codes (`bool`) for success/failure.
- If your device exposes custom services, add matching XML files alongside
  the existing `service.xml` files and register them in `setupServicesImpl`.

If you'd like, I can scaffold a new header-only device template `MyDevice.h`
in this folder using `MediaRenderer` as the starting point. Example header
template:

```cpp
// MyDevice.h - header-only DLNA device template
#pragma once

#include "dlna/DLNADeviceMgr.h"

namespace tiny_dlna {

class MyDevice : public DLNADevice {
 public:
	MyDevice() {
		setSerialNumber("uuid:my-device-0001");
		setDeviceType("urn:schemas-upnp-org:device:MyDevice:1");
		setFriendlyName("MyDevice");
		setManufacturer("MyCompany");
		setModelName("MyDevice");
		setBaseURL("http://localhost:44757");
	}

	bool begin() {
		// initialise resources (pipelines, streams)
		return true;
	}

	void end() {
		// cleanup resources
	}

	void loop() {
		// periodically called by device manager
	}

 protected:
	void setupServices(HttpServer& server, IUDPService& udp) {
		// register service XML and control handlers here
	}
};

}  // namespace tiny_dlna
```