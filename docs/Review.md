# Review Summary

Source: [arduino-dlna discussion #8](https://github.com/pschatzmann/arduino-dlna/discussions/8) — reporter's modified files and audio-renderer sketch, tested against `Hi-Fi Cast` on Android.

## Applied Changes

| Change | File(s) | Notes |
|---|---|---|
| Read SOAP action body by `Content-Length` instead of looping until socket read timeout | [`src/dlna/devices/DLNADevice.h`](../src/dlna/devices/DLNADevice.h) (`parseActionRequest`) | Likely root cause of the reported "gets stuck for 30s on play/pause/track-change". Falls back to the old timeout-based read when `Content-Length` is missing (the reporter's version dropped that fallback, which we fixed). Header parsing hardened with `strtol` after code review found `atoi()` silently treated a malformed/non-numeric header as an empty body. |
| Register each service type as an M-SEARCH target | [`src/dlna/devices/DLNADevice.h`](../src/dlna/devices/DLNADevice.h) (`setupParser`) | Lets control points (e.g. VLC) that search by service type get a response. |
| Accept discovery by UDN (`ST: uuid:...`) | [`src/dlna/common/Schedule.h`](../src/dlna/common/Schedule.h) (`isValidSearchTarget`) | Some control points (BubbleUPnP, some Windows components) search this way. |
| `MULTI_MSG_DELAY_MS` 80 → 50 | [`src/dlna/common/Schedule.h`](../src/dlna/common/Schedule.h) | SSDP multi-message send delay tuning, applied on request. |
| `LastChange` eventing state variable added; individual AVTransport/RenderingControl variables set to `sendEvents="no"` | [`src/dlna/devices/MediaRenderer/DLNAMediaRendererDescr.h`](../src/dlna/devices/MediaRenderer/DLNAMediaRendererDescr.h) | Brings the SCPD description in line with what the library already does on the wire — `SubscriptionMgrDevice.h` only ever sends `<LastChange>`-wrapped GENA events, never per-variable events. Standard AVTransport:1/RenderingControl:1 pattern. |
| `DLNA_HTTP_READ_TIMEOUT_MS` → 300 | [`src/dlna_config.h`](../src/dlna_config.h) | Reporter's tested value; kept at 300ms after code review flagged the tradeoff (see below) — decision confirmed. |
| `Connection: keep-alive` vs `close` made configurable | [`src/http/Server/HttpClientHandler.h`](../src/http/Server/HttpClientHandler.h), [`src/http/Server/IHttpServer.h`](../src/http/Server/IHttpServer.h), [`src/http/Server/HttpServer.h`](../src/http/Server/HttpServer.h) | New `setKeepAlive(bool)` on `IHttpServer`/`HttpServer` (default `true`, unchanged behavior). Sketch can call `server.setKeepAlive(false)` if needed. Reporter's version hardcoded `close`, which is a real behavioral tradeoff, not a strict bugfix — configurability avoids forcing that choice on everyone. |
| `dlna:X_DLNACAP` capability + `xmlns:dlna` namespace made configurable | [`src/dlna_config.h`](../src/dlna_config.h) (`DEFAULT_DLNA_XCAP "audio"`), [`src/dlna/common/DLNADeviceInfo.h`](../src/dlna/common/DLNADeviceInfo.h) (`setDLNACapability()`/`getDLNACapability()`) | Reporter's version hardcoded `X_DLNACAP="audio"` directly in the shared base class used by all device types (media servers included), which is too specific to be a safe default. Made it a configurable field defaulting to `"audio"` instead; pass `""` to omit the element. Also fixed the copy constructor, which wasn't copying `ns` at all previously. |

All changes were verified against a full `cmake --build` of the library and example sketches, and the generated device-description XML was inspected directly to confirm the new namespace/capability output.

## Proposed Changes Not Applied

| Proposal | Why not applied as-is | How to achieve the same result |
|---|---|---|
| Case-insensitive `ST:` header parsing in `DLNAControlPointRequestParser.h` | The reporter's version also silently dropped parsing of `Location:` and `USN:` from M-SEARCH replies, which looks like an unintentional regression — a control point needs `Location` to fetch a discovered device's description XML at all. | **Already covered.** The shared `parse()` helper in that file already lowercases both the search tag and the input before matching, so `ST:`, `st:`, `St:`, etc. are already handled — verified independently with a standalone test using the same matching logic. No code change was needed. If you also want to tolerate a stray space before the colon (`ST :`), that's a distinct, much rarer edge case — say the word and it can be added without touching the `Location`/`USN` parsing. |
| Hardcoded `dlna:X_DLNACAP = "audio"` in the shared device-info base class | Too specific for a class also used by non-renderer devices (e.g. media servers, or renderers that aren't audio-only). | **Resolved differently, see "Applied Changes" above** — added as a configurable field (`setDLNACapability()`) defaulting to `"audio"` via `DEFAULT_DLNA_XCAP`, so the common case works out of the box while other device types can override or clear it. |

### Known tradeoff, accepted as-is

`DLNA_HTTP_READ_TIMEOUT_MS` at 300ms also gates the HTTP *header* read and any fallback body read (when `Content-Length` is absent) — on a slow/congested link where TCP segments arrive further apart than 300ms, a read could in principle come back empty mid-request. This was flagged by code review; you confirmed 300ms should stay, since it matches what the reporter tested successfully on their hardware and the Content-Length-based body read (see Applied Changes) removes most of the original exposure. If read truncation is ever observed in the field, splitting this into two separate values — a short header-read timeout and a longer body-read timeout — would let both goals be met independently.

## Verification

- `cmake --build build -j4` — clean build of the library, emulator, and all example sketches after every change.
- Direct inspection of `device-xml` example output confirmed the emitted XML contains the expected `xmlns:dlna="urn:schemas-dlna-org:device-1-0"` and `<dlna:X_DLNACAP>audio</dlna:X_DLNACAP>`.
- Standalone test (`g++`, matching-logic reproduction) confirmed `ST:` header matching is already case-insensitive.
- Code review pass (`/code-review`) on the full diff surfaced and fixed one further bug: a malformed/non-numeric `Content-Length` header was being silently treated by `atoi()` as a zero-length body, skipping the SOAP body read entirely. Now uses `strtol` with explicit validation and falls back to the timeout-based read when the header is missing or unparseable.
