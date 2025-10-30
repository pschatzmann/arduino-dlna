# Digital Living Network Alliance (DLNA)


## Brief

This library provides the functionality to implement __UPnP/DLNA devices and control points__ for Arduino.

The Digital Living Network Alliance (DLNA) aimed to establish __interoperability__ among PCs, consumer appliances, and mobile devices across wired and wireless networks. The goal was to provide a common solution for __sharing digital media and content__ services. 

Though this technology is now more then 20 years old and can be considered obsolete, it is still quite useful for some use cases: For Audio and Video the successor protocols are __Chromecast and AirPlay__. Chromecast is close sourced and uses secure channels to Google, so you can't implement your own devices. On the other hand, Airplay is Apple specific and is not available on other platforms (e.g. Android). This leaves __DLNA as the only open and flexible Standard__ for accessing and sharing digital media (audio, video, images...)!

## The Standard

Reference documentation can be hard to find. Therefore I am providing some useful links:

- [UPnP and UDA Tutorial](https://upnp.org/resources/documents/UPnP_UDA_tutorial_July2014.pdf)
- [UPnP Device Architecture Tutorial](
https://embeddedinn.wordpress.com/tutorials/upnp-device-architecture/)
- [UPnP Device Architecture 1.1](http://www.upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf)
- [UPnP Device Architecture 2.0](https://openconnectivity.org/upnp-specs/UPnP-arch-DeviceArchitecture-v2.0-20200417.pdf)


I struggled quite a bit to choose the right approach to implement this on Arduino. On the desktop, similar functionality can be provided e.g. with the following projects:

- [pupnp](https://github.com/pupnp/pupnp) A Portable SDK for UPnP* Devices
- [GMediaRender](https://gmrender.nongnu.org/): A headless UPnP/DLNA media renderer 
- [minidlna](https://wiki.debian.org/minidlna) A minimal UPnP/DLNA media server

This can be quite useful for testing the functionality.


## Device Types and Control Points

This library provides memory-efficient classes to implement __DLNA devices__ and __control points__ (=clients). In addition to the core functionality, this project includes some easy-to-use classes:

- Devices
  - [DLNADevice](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADevice.html) (Generic Device API)
  - [DLNAMediaRenderer](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAMediaRenderer.html)
  - [DLNAMediaServer](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAMediaServer.html)

- Control Points
  - [DLNAControlPoint](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPoint.html) (Generic Control Point API)
  - [DLNAControlPointMediaRenderer](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPointMediaRenderer.html)
  - [DLNAControlPointMediaServer](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPointMediaServer.html)


## Implementation Approach

A DLNA device uses __UDP, HTTP, XML and SOAP__ to discover and manage services, which adds quite some complexity.

I implemented the functionality from scratch using the __standart Arduino Network API__ and avoided external dependencies.

The [DLNAControlPoint](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNAControlPoint.html) sets up a control point and lets you execute actions.
The [DLNADevice](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADevice.html) class provides the setup for a basic DLNA device service. Devices are represented by the [DLNADeviceInfo](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADeviceInfo.html) class. A device registers itself on the network and answers UDP DLNA queries and requests:

- UDP communication is handled via a [Scheduler](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1Scheduler.html) and a [Device Request Parser](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1DLNADeviceRequestParser.html).
- HTTP requests are handled with the bundled [TinyHttp Server](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1HttpServer.html).
- XML service descriptions can be stored as char arrays in PROGMEM or generated dynamically using the [XMLPrinter](https://pschatzmann.github.io/arduino-dlna/structtiny__dlna_1_1XMLPrinter.html) class.
- XML Parsing can be done using the [XMLParser](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1XMLParser.html), [XMLParserPrint](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1XMLParserPrint.html) and [XMLAttributeParser](https://pschatzmann.github.io/arduino-dlna/classtiny__dlna_1_1XMLAttributeParser.html)

Developing and debugging on a microcontroller is tedious; therefore this project can also be compiled and run on a __Linux os OS X desktop__.

## Project Documentation

* [Wiki](https://github.com/pschatzmann/arduino-dlna/wiki)
* [Class Documentation](https://pschatzmann.github.io/arduino-dlna/annotated.html)


