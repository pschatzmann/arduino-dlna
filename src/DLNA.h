#pragma once
#include "dlna/DLNAControlPoint.h"
#include "dlna/DLNADevice.h"
#include "dlna/devices/MediaRenderer/MediaRenderer.h"
#include "dlna/udp/UDPService.h"
#include "dlna/clients/ControlPointMediaRenderer.h"
#include "dlna/clients/ControlPointMediaServer.h"
#include "dlna/devices/MediaServer/MediaServer.h"
#include "dlna/devices/MediaRenderer/MediaRenderer.h"
#ifndef IS_DESKTOP
#  include "dlna/udp/UDPAsyncService.h"
#endif

using namespace tiny_dlna;

#ifdef IS_DESKTOP
using UDPAsyncService = UDPService<WiFiUDP>;
#endif
