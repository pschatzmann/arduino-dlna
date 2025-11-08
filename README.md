# Digital Living Network Alliance (DLNA)


## Brief

This library provides the functionality to implement __UPnP/DLNA devices and control points__ for Arduino.

The Digital Living Network Alliance (DLNA) aimed to establish __interoperability__ among PCs, consumer appliances, and mobile devices across wired and wireless networks. The goal was to provide a common solution for __sharing digital media and content__ services. 

Though this technology is now more then 20 years old and can be considered obsolete, it is still quite useful for some use cases: For __Audio and Video__ the successor protocols are __Chromecast and AirPlay__. Chromecast is close sourced and uses secure channels to Google, so you can't implement your own devices. On the other hand, Airplay is Apple specific and is not available on other platforms (e.g. Android). This leaves __DLNA as the only open and flexible Standard__ for accessing and sharing digital media (audio, video, images...)!


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


This implementation supports the different Arduino network stacks. Just specify
your network class as template parameter. The table below shows the recommended
pairings for the common Arduino WiFi and Ethernet libraries.

| Class | WiFi | Ethernet |
| - | - | - |
| `HttpRequest` | `tiny_dlna::HttpRequest<WiFiClient>` | `tiny_dlna::HttpRequest<EthernetClient>` |
| `HttpServer` | `tiny_dlna::HttpServer<WiFiClient, WiFiServer>`  | `tiny_dlna::HttpServer<EthernetClient, EthernetServer>` |
| `DLNADevice` | `tiny_dlna::DLNADevice<WiFiClient>` | `tiny_dlna::DLNADevice<EthernetClient>` |
| `DLNAMediaRenderer` | `tiny_dlna::DLNAMediaRenderer<WiFiClient>` | `tiny_dlna::DLNAMediaRenderer<EthernetClient>` |
| `DLNAMediaServer` | `tiny_dlna::DLNAMediaServer<WiFiClient>` | `tiny_dlna::DLNAMediaServer<EthernetClient>` |
| `UDPService` | `tiny_dlna::UDPService<WiFiUDP>` | `tiny_dlna::UDPService<EthernetUDP>` |



## Project Documentation

* [Wiki](https://github.com/pschatzmann/arduino-dlna/wiki)
* [Class Documentation](https://pschatzmann.github.io/arduino-dlna/annotated.html)


## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-dlna.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.


## Sponsor Me

This software is totally free, but you can make me happy by rewarding me with a treat

- [Buy me a coffee](https://www.buymeacoffee.com/philschatzh)
- [Paypal me](https://paypal.me/pschatzmann?country.x=CH&locale.x=en_US)