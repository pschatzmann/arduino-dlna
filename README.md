# Digital Living Network Alliance (DLNA)


## Brief

This example demonstrates the process of running a UPnP/DLNA __media renderer__ in Arduino.

The Digital Living Network Alliance (DLNA) aims to establish the interoperability among PCs, consumer appliances, and mobile devices in wireless networks and wired networks. The purpose is to provide a solution for sharing between digital media and content services.

Reference Documentation is hard to find. Therfore I am providing some valuable links:

- [UPnP and UDA Tutorial](https://upnp.org/resources/documents/UPnP_UDA_tutorial_July2014.pdf)
- [UPnP Device Architecture Tutorial](
https://embeddedinn.wordpress.com/tutorials/upnp-device-architecture/)
- [UPnP Device Architecture](http://www.upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf)

https://openconnectivity.org/upnp-specs/UPnP-arch-DeviceArchitecture-v1.1.pdf

I was struggling quite a bit to choose the right approach to implement something like this in Arduino. 
On the desktop the the functionality can be provided with the help of the following projects:

- [pupnp](https://github.com/pupnp/pupnp) A Portable SDK for UPnP* Devices
- [gmrender-resurrect](https://github.com/hzeller/gmrender-resurrect): A headless UPnP/DLNA media renderer based on pupnp

## Implementation Approach

A DLNA device uses UDP, Http, XML and Soap to discover and manage the services, so there is quite some complexity involved. 

I decided to implement the functionality from scratch using the basic Arduino Network API and I do not rely on any external libraries.

The [DLNADevice](https://pschatzmann.github.io/arduino-dlna-server/docs/html/classtiny__dlna_1_1DLNADevice.html) class provides the setup of a Basic DLNA Device service. Devices are represented by the [DLNADeviceInfo](https://pschatzmann.github.io/arduino-dlna-server/docs/html/classtiny__dlna_1_1DLNADeviceInfo.html) class. The device registers itself to the network and answers to the UDP DLNA queries and requests:

- We handle the UDP communication via a [Scheduler](https://pschatzmann.github.io/arduino-dlna-server/docs/html/classtiny__dlna_1_1Scheduler.html) and a [Request Parser](https://pschatzmann.github.io/arduino-dlna-server/docs/html/classtiny__dlna_1_1DLNARequestParser.html)
- We handle the Http requests with the help of my [TinyHttp Server](https://pschatzmann.github.io/arduino-dlna-server/docs/html/classtiny__dlna_1_1HttpServer.html)
- The XML service descriptions can be stored as char arrays in progmem or
  generated dynamically with the help of the [XMLPrinter](https://pschatzmann.github.io/arduino-dlna-server/docs/html/structtiny__dlna_1_1XMLPrinter.html) class.

Developping and debugging on a microcontroller is quite tedious: therefore this project can also be compiled and run on a __linux desktop__.

## Project Documentation

- [Class Documentation](https://pschatzmann.github.io/arduino-dlna-server/docs/html/annotated.html)