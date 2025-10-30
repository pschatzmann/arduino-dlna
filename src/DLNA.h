#pragma once
#include "dlna/DLNAControlPoint.h"
#include "dlna/DLNADevice.h"
#include "dlna/udp/UDPService.h"
#include "dlna/clients/DLNAControlPointMediaRenderer.h"
#include "dlna/clients/DLNAControlPointMediaServer.h"
#include "dlna/devices/MediaServer/DLNAMediaServer.h"
#include "dlna/devices/MediaRenderer/DLNAMediaRenderer.h"
#ifndef IS_DESKTOP
#  include "dlna/udp/UDPAsyncService.h"
#endif

using namespace tiny_dlna;

#ifdef IS_DESKTOP
using UDPAsyncService = UDPService<WiFiUDP>;
#endif
