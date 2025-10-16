# Digital Living Network Alliance (DLNA)


## Brief

This library provides the functionality to implement a UPnP/DLNA [device](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNADeviceMgr.html) and a [control point](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNAControlPointMgr.html) for Arduino.

The Digital Living Network Alliance (DLNA) aims to establish interoperability among PCs, consumer appliances, and mobile devices across wired and wireless networks. The goal is to provide a common solution for sharing digital media and content services.

Reference documentation can be hard to find. Therefore I am providing some useful links:

- [UPnP and UDA Tutorial](https://upnp.org/resources/documents/UPnP_UDA_tutorial_July2014.pdf)
- [UPnP Device Architecture Tutorial](
https://embeddedinn.wordpress.com/tutorials/upnp-device-architecture/)
- [UPnP Device Architecture](http://www.upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf)

I struggled to choose the right approach to implement this on Arduino.
On the desktop, similar functionality can be provided with the following projects:

- [pupnp](https://github.com/pupnp/pupnp) A Portable SDK for UPnP* Devices
- [gmrender-resurrect](https://github.com/hzeller/gmrender-resurrect): A headless UPnP/DLNA media renderer based on pupnp

This library provides memory-efficient classes to implement DLNA devices and clients (control points).

## Device Types and Control Points

In addition to the core functionality, this project includes some easy-to-use classes:

- Devices
  - MediaRenderer
  - MediaServer
- Control Points
  - ControlPointMediaRenderer
  - ControlPointMediaServer


## Implementation Approach

A DLNA device uses UDP, HTTP, XML and SOAP to discover and manage services, which adds complexity.

I implemented the functionality from scratch using the basic Arduino network API and avoided external dependencies where possible.

The [DLNAControlPointMgr](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNAControlPointMgr.html) sets up a control point and lets you execute actions.
The [DLNADeviceMgr](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNADeviceMgr.html) class provides the setup for a basic DLNA device service. Devices are represented by the [DLNADevice](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNADevice.html) class. A device registers itself on the network and answers UDP DLNA queries and requests:

- UDP communication is handled via a [Scheduler](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1Scheduler.html) and a [Request Parser](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1DLNARequestParser.html).
- HTTP requests are handled with the bundled [TinyHttp Server](https://pschatzmann.github.io/arduino-dlna/docs/html/classtiny__dlna_1_1HttpServer.html).
- XML service descriptions can be stored as char arrays in progmem or generated dynamically using the [XMLPrinter](https://pschatzmann.github.io/arduino-dlna/docs/html/structtiny__dlna_1_1XMLPrinter.html) class.

Developing and debugging on a microcontroller is tedious; therefore this project can also be compiled and run on a __Linux desktop__.

## Project Documentation

* [Wiki](https://github.com/pschatzmann/arduino-dlna/wiki)
* [Class Documentation](https://pschatzmann.github.io/arduino-dlna/docs/html/annotated.html)


