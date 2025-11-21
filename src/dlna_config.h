#pragma once

/// Define delay in ms for main DLNA loop
#ifndef DLNA_LOOP_DELAY_MS
#define DLNA_LOOP_DELAY_MS 5
#endif

/// Define scheduler run interval in ms
#ifndef DLNA_RUN_SCHEDULER_EVERY_MS
#define DLNA_RUN_SCHEDULER_EVERY_MS 10
#endif

/// Define subscription publish interval in ms
#ifndef DLNA_RUN_SUBSCRIPTIONS_EVERY_MS
#define DLNA_RUN_SUBSCRIPTIONS_EVERY_MS 10
#endif

/// Define the default http request timeout
#ifndef DLNA_HTTP_REQUEST_TIMEOUT_MS
#define DLNA_HTTP_REQUEST_TIMEOUT_MS 60
#endif

/// Define XML parse buffer size
#ifndef XML_PARSER_BUFFER_SIZE
#define XML_PARSER_BUFFER_SIZE 512
#endif

/// Define initial size for StrPrint
#ifndef STR_PRINT_INITIAL_SIZE
#define STR_PRINT_INITIAL_SIZE 200
#endif

/// Define increment size for StrPrint
#ifndef STR_PRINT_INC_SIZE
#define STR_PRINT_INC_SIZE 200
#endif

/// SSDP multicast/UDP port (default 1900)
#ifndef DLNA_SSDP_PORT
#define DLNA_SSDP_PORT 1900
#endif

/// app-wide max URL length
#ifndef DLNA_MAX_URL_LEN
#define DLNA_MAX_URL_LEN 256
#endif

/// Max printf buffer size
#ifndef MAX_PRINTF_SIZE
#define MAX_PRINTF_SIZE 512
#endif

/// All possible protocols
#ifndef DLNA_PROTOCOL_ALL
#define DLNA_PROTOCOL_ALL                                                    \
  "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN,http-get:*:image/"              \
  "jpeg:DLNA.ORG_PN=JPEG_SM,http-get:*:image/"                               \
  "jpeg:DLNA.ORG_PN=JPEG_MED,http-get:*:image/"                              \
  "jpeg:DLNA.ORG_PN=JPEG_LRG,http-get:*:video/"                              \
  "mpeg:DLNA.ORG_PN=AVC_TS_HD_50_AC3_ISO,http-get:*:video/"                  \
  "mpeg:DLNA.ORG_PN=AVC_TS_HD_60_AC3_ISO,http-get:*:video/"                  \
  "mpeg:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_ISO,http-get:*:video/"                  \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_ISO,http-get:*:video/"            \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO,http-get:*:video/"                  \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_ISO,http-get:*:video/"             \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_ISO,http-get:*:video/"            \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_ISO,http-get:*:video/"                  \
  "mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_ISO,http-get:*:video/"             \
  "mpeg:DLNA.ORG_PN=MPEG_PS_NTSC,http-get:*:video/"                          \
  "mpeg:DLNA.ORG_PN=MPEG_PS_PAL,http-get:*:video/"                           \
  "mpeg:DLNA.ORG_PN=MPEG_TS_HD_NA_ISO,http-get:*:video/"                     \
  "mpeg:DLNA.ORG_PN=MPEG_TS_SD_NA_ISO,http-get:*:video/"                     \
  "mpeg:DLNA.ORG_PN=MPEG_TS_SD_EU_ISO,http-get:*:video/"                     \
  "mpeg:DLNA.ORG_PN=MPEG1,http-get:*:video/"                                 \
  "mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AAC_MULT5,http-get:*:video/"                \
  "mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AC3,http-get:*:video/"                      \
  "mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF15_AAC_520,http-get:*:video/"               \
  "mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF30_AAC_940,http-get:*:video/"               \
  "mp4:DLNA.ORG_PN=AVC_MP4_BL_L31_HD_AAC,http-get:*:video/"                  \
  "mp4:DLNA.ORG_PN=AVC_MP4_BL_L32_HD_AAC,http-get:*:video/"                  \
  "mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC,http-get:*:video/"                  \
  "mp4:DLNA.ORG_PN=AVC_MP4_HP_HD_AAC,http-get:*:video/"                      \
  "mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_1080i_AAC,http-get:*:video/"                \
  "mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_720p_AAC,http-get:*:video/"                 \
  "mp4:DLNA.ORG_PN=MPEG4_P2_MP4_ASP_AAC,http-get:*:video/"                   \
  "mp4:DLNA.ORG_PN=MPEG4_P2_MP4_SP_VGA_AAC,http-get:*:video/"                \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3,http-get:*:video/"         \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3_T,http-get:*:video/"       \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3,http-get:*:video/"         \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3_T,http-get:*:video/"       \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_T,http-get:*:video/"       \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5,http-get:*:video/"   \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_T,http-get:*:video/" \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3,http-get:*:video/"         \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_T,http-get:*:video/"       \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3,http-get:*:video/"    \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_T,http-get:*:video/"  \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5,http-get:*:video/"   \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_T,http-get:*:video/" \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3,http-get:*:video/"         \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_T,http-get:*:video/"       \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3,http-get:*:video/"    \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_T,http-get:*:video/"  \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA,http-get:*:video/"            \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA_T,http-get:*:video/"          \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU,http-get:*:video/"            \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU_T,http-get:*:video/"          \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA,http-get:*:video/"            \
  "vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA_T,http-get:*:video/"          \
  "x-ms-wmv:DLNA.ORG_PN=WMVSPLL_BASE,http-get:*:video/"                      \
  "x-ms-wmv:DLNA.ORG_PN=WMVSPML_BASE,http-get:*:video/"                      \
  "x-ms-wmv:DLNA.ORG_PN=WMVSPML_MP3,http-get:*:video/"                       \
  "x-ms-wmv:DLNA.ORG_PN=WMVMED_BASE,http-get:*:video/"                       \
  "x-ms-wmv:DLNA.ORG_PN=WMVMED_FULL,http-get:*:video/"                       \
  "x-ms-wmv:DLNA.ORG_PN=WMVMED_PRO,http-get:*:video/"                        \
  "x-ms-wmv:DLNA.ORG_PN=WMVHIGH_FULL,http-get:*:video/"                      \
  "x-ms-wmv:DLNA.ORG_PN=WMVHIGH_PRO,http-get:*:video/"                       \
  "3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AAC,http-get:*:video/"              \
  "3gpp:DLNA.ORG_PN=MPEG4_P2_3GPP_SP_L0B_AMR,http-get:*:audio/"              \
  "mpeg:DLNA.ORG_PN=MP3,http-get:*:audio/"                                   \
  "x-ms-wma:DLNA.ORG_PN=WMABASE,http-get:*:audio/"                           \
  "x-ms-wma:DLNA.ORG_PN=WMAFULL,http-get:*:audio/"                           \
  "x-ms-wma:DLNA.ORG_PN=WMAPRO,http-get:*:audio/"                            \
  "x-ms-wma:DLNA.ORG_PN=WMALSL,http-get:*:audio/"                            \
  "x-ms-wma:DLNA.ORG_PN=WMALSL_MULT5,http-get:*:audio/"                      \
  "mp4:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/"                            \
  "3gpp:DLNA.ORG_PN=AAC_ISO_320,http-get:*:audio/"                           \
  "mp4:DLNA.ORG_PN=AAC_ISO,http-get:*:audio/"                                \
  "mp4:DLNA.ORG_PN=AAC_MULT5_ISO,http-get:*:audio/"                          \
  "L16;rate=44100;channels=2:DLNA.ORG_PN=LPCM,http-get:*:image/"             \
  "jpeg:*,http-get:*:video/avi:*,http-get:*:video/divx:*,http-get:*:video/"  \
  "x-matroska:*,http-get:*:video/mpeg:*,http-get:*:video/"                   \
  "mp4:*,http-get:*:video/x-ms-wmv:*,http-get:*:video/"                      \
  "x-msvideo:*,http-get:*:video/x-flv:*,http-get:*:video/"                   \
  "x-tivo-mpeg:*,http-get:*:video/quicktime:*,http-get:*:audio/"             \
  "mp4:*,http-get:*:audio/x-wav:*,http-get:*:audio/"                         \
  "x-flac:*,http-get:*:audio/x-dsd:*,http-get:*:application/"                \
  "ogg:*http-get:*:application/vnd.rn-realmedia:*http-get:*:application/"    \
  "vnd.rn-realmedia-vbr:*http-get:*:video/webm:*"
#endif

/// All possible audio protocols
#ifndef DLNA_PROTOCOL_AUDIO
#define DLNA_PROTOCOL_AUDIO                                      \
  "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3,"                       \
  "http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMABASE,"               \
  "http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAFULL,"               \
  "http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAPRO,"                \
  "http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMALSL,"                \
  "http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMALSL_MULT5,"          \
  "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO_320,"                \
  "http-get:*:audio/3gpp:DLNA.ORG_PN=AAC_ISO_320,"               \
  "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO,"                    \
  "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_MULT5_ISO,"              \
  "http-get:*:audio/L16;rate=44100;channels=2:DLNA.ORG_PN=LPCM," \
  "http-get:*:audio/x-wav:*,"                                    \
  "http-get:*:audio/x-flac:*,"                                   \
  "http-get:*:audio/x-dsd:*"
#endif

/// Define maximum number of notify retries
#ifndef MAX_NOTIFY_RETIES
#define MAX_NOTIFY_RETIES 3
#endif

/// Define the default protocols to use
#ifndef DLNA_PROTOCOL
#define DLNA_PROTOCOL DLNA_PROTOCOL_AUDIO
#endif

/// Define the default allocator base class to use: std::allocator, AllocatorPSRAM
#ifndef DLNA_ALLOCATOR
    #define DLNA_ALLOCATOR AllocatorPSRAM
#endif

/// Enable or disable logging of XML messages
#ifndef DLNA_LOG_XML
#define DLNA_LOG_XML false
#endif

#ifndef DLNA_CHECK_XML_LENGTH
#define DLNA_CHECK_XML_LENGTH false
#endif

/// Define the netmask for discovery filtering
#ifndef DLNA_DISCOVERY_NETMASK
#define DLNA_DISCOVERY_NETMASK IPAddress(255, 255, 255, 0)
#endif

// Deactivate warnings
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
