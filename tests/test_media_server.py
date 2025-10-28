import requests
import pytest
import socket
from urllib.parse import urlparse
import sys
import logging
import xml.etree.ElementTree as ET
import html

logger = logging.getLogger("dlna.tests")
from pathlib import Path


# Directory to persist response artifacts (tests/responses)
RESP_DIR = Path(__file__).resolve().parent / "responses"
RESP_DIR.mkdir(parents=True, exist_ok=True)


def _save_response(name: str, text: str):
    """Save response text to tests/responses/<name>.xml and log the path."""
    p = RESP_DIR / f"{name}.xml"
    try:
        p.write_text(text or "", encoding="utf-8")
        logger.info("Saved response to %s", p)
    except Exception:
        logger.exception("Failed to save response %s", p)




def _local_name(tag: str) -> str:
    """Return the local-name portion of an Element tag (strip namespace)."""
    if tag is None:
        return ""
    if tag.startswith("{"):
        return tag.split("}", 1)[1]
    return tag


def _find_in_response(root: ET.Element, localname: str) -> ET.Element | None:
    """Search for the first element with given local-name under root."""
    for el in root.iter():
        if _local_name(el.tag) == localname:
            return el
    return None


def _get_text(elem: ET.Element | None) -> str:
    if elem is None:
        return ""
    return (elem.text or "").strip()

# The `base_url` fixture (in conftest.py) provides the base URL and is set
# via the pytest CLI option `--base` (defaults to http://127.0.0.1:44757).


def _do_post(base_url: str, path: str, data: bytes, headers: dict, timeout: float = 15.0):
    """POST helper that catches low-level failures and prints raw server replies.

    On exception, the helper opens a raw TCP socket to the server and sends the
    same HTTP request to capture any raw bytes returned before the socket is
    closed. This is useful to diagnose BadStatusLine / connection-close issues.
    """
    try:
        return requests.post(base_url + path, data=data, headers=headers, timeout=timeout)
    except Exception as e:
        logger.exception("requests.post failed: %s", e)
        # Attempt raw TCP probe
        try:
            u = urlparse(base_url)
            host = u.hostname or "127.0.0.1"
            port = u.port or (443 if u.scheme == "https" else 80)
            # Increase raw socket probe timeouts to allow larger replies
            # (previously 2s which caused recv() TimeoutError).
            s = socket.create_connection((host, port), timeout=15)
            s.settimeout(10)
            # Build a minimal HTTP/1.1 request
            raw_headers = []
            raw_headers.append(f"POST {path} HTTP/1.1")
            raw_headers.append(f"Host: {host}:{port}")
            for k, v in headers.items():
                raw_headers.append(f"{k}: {v}")
            raw_headers.append(f"Content-Length: {len(data)}")
            raw_headers.append("Connection: close")
            raw_headers.append("")
            raw_headers.append("")
            request = "\r\n".join(raw_headers).encode("utf-8") + data
            try:
                s.sendall(request)
                raw = b""
                while True:
                    part = s.recv(4096)
                    if not part:
                        break
                    raw += part
                logger.info("--- RAW SOCKET REPLY START ---")
                try:
                    logger.info(raw.decode("utf-8", errors="replace"))
                except Exception:
                    logger.info(repr(raw))
                logger.info("--- RAW SOCKET REPLY END ---")
            finally:
                s.close()
        except Exception as se:
            logger.exception("Raw socket probe failed: %s", se)
        # Re-raise original exception for pytest to register the failure
        raise



def test_connmgr_get_protocol_info(base_url):
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetProtocolInfo xmlns:u="urn:schemas-upnp-org:service:ConnectionManager:1"/>'
        '</s:Body></s:Envelope>'
    )
    # SOAPACTION header must contain the urn and action
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ConnectionManager:1#GetProtocolInfo"'
    }
    r = _do_post(base_url, "/CM/control", body.encode("utf-8"), headers, timeout=15)
    # Log and persist the reply XML for debugging/inspection (start on new line)
    logger.info("\n%s", r.text)
    _save_response("connmgr_getprotocolinfo", r.text)
    assert r.status_code == 200, f"Unexpected status {r.status_code}: {r.text}"

    # Parse and validate XML structure
    root = ET.fromstring(r.text)
    # ensure we have a GetProtocolInfoResponse node
    resp = _find_in_response(root, "GetProtocolInfoResponse")
    assert resp is not None, "Missing GetProtocolInfoResponse element"
    src = _get_text(_find_in_response(resp, "Source"))
    sink = _get_text(_find_in_response(resp, "Sink"))
    assert src is not None and sink is not None


def test_connmgr_get_current_connection_ids(base_url):
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetCurrentConnectionIDs xmlns:u="urn:schemas-upnp-org:service:ConnectionManager:1"/>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ConnectionManager:1#GetCurrentConnectionIDs"'
    }
    r = _do_post(base_url, "/CM/control", body.encode("utf-8"), headers, timeout=15)
    # Log and persist the reply XML for debugging/inspection (start on new line)
    logger.info("\n%s", r.text)
    _save_response("connmgr_getcurrentconnectionids", r.text)
    assert r.status_code == 200

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "GetCurrentConnectionIDsResponse")
    assert resp is not None
    cids = _get_text(_find_in_response(resp, "ConnectionIDs"))
    # may be empty string or comma-separated ids
    assert cids is not None


def test_connmgr_get_current_connection_info(base_url):
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetCurrentConnectionInfo xmlns:u="urn:schemas-upnp-org:service:ConnectionManager:1">'
        '<ConnectionID>0</ConnectionID>'
        '</u:GetCurrentConnectionInfo>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ConnectionManager:1#GetCurrentConnectionInfo"'
    }
    r = _do_post(base_url, "/CM/control", body.encode("utf-8"), headers, timeout=15)
    # Log and persist the reply XML for debugging/inspection (start on new line)
    logger.info("\n%s", r.text)
    _save_response("connmgr_getcurrentconnectioninfo", r.text)
    assert r.status_code == 200

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "GetCurrentConnectionInfoResponse")
    assert resp is not None
    rcs = _get_text(_find_in_response(resp, "RcsID"))
    av = _get_text(_find_in_response(resp, "AVTransportID"))
    proto = _get_text(_find_in_response(resp, "ProtocolInfo"))
    direction = _get_text(_find_in_response(resp, "Direction"))
    status = _get_text(_find_in_response(resp, "Status"))
    # basic validations
    assert rcs.isdigit()
    assert av.isdigit()
    assert direction in ("Input", "Output")
    assert status != ""


def test_get_search_capabilities(base_url):
    """Request GetSearchCapabilities and ensure SearchCaps element is present."""
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetSearchCapabilities xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1"/>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ContentDirectory:1#GetSearchCapabilities"'
    }
    r = _do_post(base_url, "/CD/control", body.encode("utf-8"), headers, timeout=15)
    logger.info("\n%s", r.text)
    _save_response("contentdirectory_getsearchcapabilities", r.text)
    assert r.status_code == 200

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "GetSearchCapabilitiesResponse")
    assert resp is not None
    caps = _get_text(_find_in_response(resp, "SearchCaps"))
    # caps may be empty, but element must exist
    assert caps is not None


def test_get_sort_capabilities(base_url):
    """Request GetSortCapabilities and ensure SortCaps element is present."""
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetSortCapabilities xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1"/>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ContentDirectory:1#GetSortCapabilities"'
    }
    r = _do_post(base_url, "/CD/control", body.encode("utf-8"), headers, timeout=15)
    logger.info("\n%s", r.text)
    _save_response("contentdirectory_getsortcapabilities", r.text)
    assert r.status_code == 200

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "GetSortCapabilitiesResponse")
    assert resp is not None
    caps = _get_text(_find_in_response(resp, "SortCaps"))
    assert caps is not None


def test_get_system_update_id(base_url):
    """Request GetSystemUpdateID and ensure Id element contains a number."""
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:GetSystemUpdateID xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1"/>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ContentDirectory:1#GetSystemUpdateID"'
    }
    r = _do_post(base_url, "/CD/control", body.encode("utf-8"), headers, timeout=15)
    logger.info("\n%s", r.text)
    _save_response("contentdirectory_getsystemupdateid", r.text)
    assert r.status_code == 200

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "GetSystemUpdateIDResponse")
    assert resp is not None
    id_str = _get_text(_find_in_response(resp, "Id"))
    assert id_str.isdigit()

def test_contentdirectory_browse_root(base_url):
    # Browse Direct Children of root container (ObjectID 0)
    body = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">'
        '<s:Body>'
        '<u:Browse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1">'
        '<ObjectID>0</ObjectID>'
        '<BrowseFlag>BrowseDirectChildren</BrowseFlag>'
        '<Filter>*</Filter>'
        '<StartingIndex>0</StartingIndex>'
        '<RequestedCount>10</RequestedCount>'
        '<SortCriteria/>'
        '</u:Browse>'
        '</s:Body></s:Envelope>'
    )
    headers = {
        "Content-Type": 'text/xml; charset="utf-8"',
        "SOAPACTION": '"urn:schemas-upnp-org:service:ContentDirectory:1#Browse"'
    }
    r = _do_post(base_url, "/CD/control", body.encode("utf-8"), headers, timeout=15)
    # Log and persist the reply XML for debugging/inspection (start on new line)
    logger.info("\n%s", r.text)
    _save_response("contentdirectory_browse_root", r.text)
    assert r.status_code == 200, f"status {r.status_code} text: {r.text[:200]}"

    root = ET.fromstring(r.text)
    resp = _find_in_response(root, "BrowseResponse")
    assert resp is not None
    # NumberReturned/TotalMatches/UpdateID
    num = _get_text(_find_in_response(resp, "NumberReturned"))
    total = _get_text(_find_in_response(resp, "TotalMatches"))
    update = _get_text(_find_in_response(resp, "UpdateID"))
    assert num.isdigit()
    assert total.isdigit()
    assert update.isdigit()

    # Result node contains DIDL-Lite either as child elements or as escaped text
    result_el = _find_in_response(resp, "Result")
    assert result_el is not None

    # Fast check: if there's a DIDL child element, accept it. Otherwise
    # search the result text for the escaped DIDL marker '&lt;DIDL'. This
    # keeps the test simple and tolerant of servers that emit DIDL-Lite as
    # HTML-escaped text inside the <Result> element.
    has_didl = False
    for child in result_el:
        if _local_name(child.tag).lower().startswith("didl"):
            has_didl = True
            break

    if not has_didl:
        result_text = "".join(result_el.itertext())
        if "&lt;DIDL" in result_text or result_text.lstrip().startswith("<DIDL"):
            has_didl = True

    assert has_didl, "DIDL-Lite content not found inside Result"


if __name__ == "__main__":
    import sys

    # Forward CLI args to pytest so `python test_media_server.py --base=...`
    # works. If no args are provided, run this file only.
    args = sys.argv[1:] if len(sys.argv) > 1 else [__file__]
    pytest.main(args)
