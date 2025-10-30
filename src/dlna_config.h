#pragma once

// Define whether to use custom allocator (true) or standard new/delete (false)
#ifndef DLNA_USE_ALLOCATOR
#define DLNA_USE_ALLOCATOR true
#endif

// Define the default allocator class to use (e.g Allocator, AllocatorExt, AllocatorPSRAM)
#ifndef DLNA_DEFAULT_ALLOCATOR
#define DLNA_DEFAULT_ALLOCATOR AllocatorExt
#endif

// Define delay in ms for main DLNA loop
#ifndef DLNA_LOOP_DELAY_MS
#define DLNA_LOOP_DELAY_MS 8
#endif

// Define the default http request timeout
#ifndef DLNA_HTTP_REQUEST_TIMEOUT_MS
#define DLNA_HTTP_REQUEST_TIMEOUT_MS 6000
#endif

// Define XML parse buffer size
#ifndef XML_PARSER_BUFFER_SIZE
#define XML_PARSER_BUFFER_SIZE 256
#endif

// Define initial size for StrPrint
#ifndef STR_PRINT_INITIAL_SIZE
#define STR_PRINT_INITIAL_SIZE 200
#endif

// Define increment size for StrPrint
#ifndef STR_PRINT_INC_SIZE
#define STR_PRINT_INC_SIZE 200
#endif

// SSDP multicast/UDP port (default 1900)
#ifndef DLNA_SSDP_PORT
#define DLNA_SSDP_PORT 1900
#endif

// Default subscription timeout in seconds and header value
#ifndef DLNA_SUBSCRIPTION_TIMEOUT_SEC
#define DLNA_SUBSCRIPTION_TIMEOUT_SEC 1800
#endif

// Default subscription timeout header value
#ifndef DLNA_SUBSCRIPTION_TIMEOUT_HEADER
#define DLNA_SUBSCRIPTION_TIMEOUT_HEADER "Second-1800"
#endif

// app-wide max URL length
#ifndef DLNA_MAX_URL_LEN
#define DLNA_MAX_URL_LEN 256
#endif

