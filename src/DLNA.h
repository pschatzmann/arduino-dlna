#pragma once
#include "dlna/DLNAControlPoint.h"
#include "dlna/DLNADevice.h"
#include "dlna/devices/MediaRenderer/MediaRenderer.h"
#include "dlna/udp/UDPService.h"
#ifndef IS_DESKTOP
#  include "dlna/udp/UDPAsyncService.h"
#endif

using namespace tiny_dlna;

#ifdef IS_DESKTOP
using UDPAsyncService = UDPService;
#endif
