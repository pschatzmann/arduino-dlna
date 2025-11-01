#pragma once
#include "dlna/clients/DLNAControlPoint.h"
#include "dlna/clients/DLNAControlPointMediaRenderer.h"
#include "dlna/clients/DLNAControlPointMediaServer.h"
#include "dlna/devices/DLNADevice.h"
#include "dlna/devices/MediaRenderer/DLNAMediaRenderer.h"
#include "dlna/devices/MediaServer/DLNAMediaServer.h"
#include "dlna/udp/UDPService.h"
#ifndef IS_DESKTOP
#include "dlna/udp/UDPAsyncService.h"
#endif

using namespace tiny_dlna;

#ifdef IS_DESKTOP
using UDPAsyncService = UDPService<WiFiUDP>;
#endif
