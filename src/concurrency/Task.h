#pragma once

#ifdef ESP32
#include "TaskRTOS.h"
#elif defined(__linux__)
#include "TaskLinux.h"
#else
#include "TaskRTOS.h"
#endif

