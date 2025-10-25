# Digital Living Network Alliance (DLNA)


## Brief

This library provides the functionality to implement a UPnP/DLNA [device](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADeviceMgr.html) and a [control point](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPointMgr.html) for Arduino.

The Digital Living Network Alliance (DLNA) aimed to establish interoperability among PCs, consumer appliances, and mobile devices across wired and wireless networks. The goal was to provide a common solution for sharing digital media and content services. Thugh this technology can be considered as obsolete, it is still quite useful for some use cases.

## The Standard

Reference documentation can be hard to find. Therefore I am providing some useful links:

- [UPnP and UDA Tutorial](https://upnp.org/resources/documents/UPnP_UDA_tutorial_July2014.pdf)
- [UPnP Device Architecture Tutorial](
https://embeddedinn.wordpress.com/tutorials/upnp-device-architecture/)
- [UPnP Device Architecture 1.1](http://www.upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf)
- [UPnP Device Architecture 2.0](https://openconnectivity.org/upnp-specs/UPnP-arch-DeviceArchitecture-v2.0-20200417.pdf)


I struggled to choose the right approach to implement this on Arduino.
On the desktop, similar functionality can be provided with the following projects:

- [pupnp](https://github.com/pupnp/pupnp) A Portable SDK for UPnP* Devices
- [gmrender-resurrect](https://github.com/hzeller/gmrender-resurrect): A headless UPnP/DLNA media renderer based on pupnp
- [usahre](https://github.com/yitsunweng/ushare) A UPnP/DLNA media server

This library provides memory-efficient classes to implement DLNA devices and clients (control points).

## Device Types and Control Points

In addition to the core functionality, this project includes some easy-to-use classes:

- Devices
  - DLNADevice (Generic Device API)
  - MediaRenderer
  - MediaServer
- Control Points
  - DLNAControlPoint (Generic Control Point API)
  - ControlPointMediaRenderer
  - ControlPointMediaServer


## Implementation Approach

A DLNA device uses UDP, HTTP, XML and SOAP to discover and manage services, which adds complexity.

I implemented the functionality from scratch using the basic Arduino network API and avoided external dependencies where possible.

The [DLNAControlPoint](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPoint.html) sets up a control point and lets you execute actions.
The [DLNADevice](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADevice.html) class provides the setup for a basic DLNA device service. Devices are represented by the [DLNADeviceInfo](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADeviceInfo.html) class. A device registers itself on the network and answers UDP DLNA queries and requests:

- UDP communication is handled via a [Scheduler](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1Scheduler.html) and a [Request Parser](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADeviceRequestParser.html).
- HTTP requests are handled with the bundled [TinyHttp Server](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1HttpServer.html).
- XML service descriptions can be stored as char arrays in progmem or generated dynamically using the [XMLPrinter](https://pschatzmann.github.io/arduino-dlna/structtiny__dlna_1_1XMLPrinter.html) class.

Developing and debugging on a microcontroller is tedious; therefore this project can also be compiled and run on a __Linux desktop__.

## Project Documentation

* [Wiki](https://github.com/pschatzmann/arduino-dlna/wiki)
* [Class Documentation](https://pschatzmann.github.io/arduino-dlna/annotated.html)


