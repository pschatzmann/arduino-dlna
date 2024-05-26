#pragma once
#include "dlna/DLNAControlPointMgr.h"
#include "dlna/DLNADeviceMgr.h"
#include "dlna/devices/MediaRenderer/MediaRenderer.h"
#include "dlna/udp/UDPService.h"
#ifndef IS_DESKTOP
#  include "dlna/udp/UDPAsyncService.h"
#endif

using namespace tiny_dlna;

#ifdef IS_DESKTOP
using UDPAsyncService = UDPService;
#endif
