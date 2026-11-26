"""Integration tests for serving media files: HEAD and Range requests.

Run against a media server which serves files, e.g. the
device-media-server-sdfat example:

    pytest -q tests/test_file_server.py --file-url=http://192.168.1.36:9000/files?ID=52

The file should be bigger than 1000 bytes. The tests only download small
ranges, so they also run quickly against a microcontroller.
"""

import re
import socket
from urllib.parse import urlparse

import pytest
import requests

TIMEOUT = 30


def _raw_request(url: str, method: str, headers: dict | None = None,
                 max_body: int = 65536) -> tuple[str, bytes]:
    """Send a request over a plain socket and return (header, body).

    Unlike requests, this shows exactly what the server sends after the
    header: needed to verify that HEAD replies have no body.
    """
    u = urlparse(url)
    path = u.path + ("?" + u.query if u.query else "")
    lines = [f"{method} {path} HTTP/1.1", f"Host: {u.netloc}",
             "Connection: close"]
    for k, v in (headers or {}).items():
        lines.append(f"{k}: {v}")
    data = ("\r\n".join(lines) + "\r\n\r\n").encode()
    with socket.create_connection((u.hostname, u.port or 80),
                                  timeout=TIMEOUT) as s:
        s.sendall(data)
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = s.recv(4096)
            if not chunk:
                break
            buf += chunk
        header, _, body = buf.partition(b"\r\n\r\n")
        # read what follows the header until the server closes
        s.settimeout(5)
        try:
            while len(body) < max_body:
                chunk = s.recv(4096)
                if not chunk:
                    break
                body += chunk
        except socket.timeout:
            pass
    return header.decode("latin-1").replace("\r\n", "\n"), body


def _get_range(url: str, range_value: str) -> requests.Response:
    return requests.get(url, headers={"Range": range_value}, timeout=TIMEOUT)


@pytest.fixture
def total_size(file_url):
    """Total size of the file, determined with a HEAD request."""
    r = requests.head(file_url, timeout=TIMEOUT)
    assert r.status_code == 200
    size = int(r.headers["Content-Length"])
    assert size > 1000, "use a file with more than 1000 bytes"
    return size


def test_head_file(file_url):
    header, body = _raw_request(file_url, "HEAD")
    assert header.startswith("HTTP/1.1 200"), header
    assert re.search(r"(?im)^Content-Length: *[1-9][0-9]*$", header), header
    assert re.search(r"(?im)^Accept-Ranges: *bytes$", header), header
    ctype = re.search(r"(?im)^Content-Type: *(\S+)$", header)
    assert ctype and "/" in ctype.group(1), header
    assert body == b"", f"HEAD reply must not have a body: {len(body)} bytes"


def test_head_matches_get_header(file_url, total_size):
    r = requests.get(file_url, headers={"Range": "bytes=0-0"},
                     timeout=TIMEOUT)
    h = requests.head(file_url, timeout=TIMEOUT)
    assert h.headers["Content-Type"] == r.headers["Content-Type"]
    assert int(h.headers["Content-Length"]) == total_size


def test_head_device_xml(file_url):
    """HEAD is also supported for handlers which were registered for GET."""
    u = urlparse(file_url)
    header, body = _raw_request(f"{u.scheme}://{u.netloc}/device.xml", "HEAD")
    assert header.startswith("HTTP/1.1 200"), header
    assert body == b""


def test_get_accept_all(file_url):
    """Media players send "Accept: */*": the file must not be rejected."""
    r = requests.get(file_url, headers={"Accept": "*/*", "Range": "bytes=0-9"},
                     timeout=TIMEOUT)
    assert r.status_code == 206
    assert not r.headers["Content-Type"].startswith("text/")


def test_range_first_bytes(file_url, total_size):
    r = _get_range(file_url, "bytes=0-99")
    assert r.status_code == 206
    assert r.headers["Content-Range"] == f"bytes 0-99/{total_size}"
    assert int(r.headers["Content-Length"]) == 100
    assert r.headers["Accept-Ranges"] == "bytes"
    assert len(r.content) == 100


def test_range_content_is_consistent(file_url):
    """Overlapping ranges must provide the same bytes."""
    a = _get_range(file_url, "bytes=0-299").content
    b = _get_range(file_url, "bytes=100-199").content
    assert len(a) == 300 and len(b) == 100
    assert a[100:200] == b


def test_range_open_end(file_url, total_size):
    start = total_size - 10
    r = _get_range(file_url, f"bytes={start}-")
    assert r.status_code == 206
    assert r.headers["Content-Range"] == (
        f"bytes {start}-{total_size - 1}/{total_size}")
    assert len(r.content) == 10


def test_range_suffix(file_url, total_size):
    r = _get_range(file_url, "bytes=-100")
    assert r.status_code == 206
    assert r.headers["Content-Range"] == (
        f"bytes {total_size - 100}-{total_size - 1}/{total_size}")
    assert len(r.content) == 100
    # same bytes as the explicit range
    r2 = _get_range(file_url, f"bytes={total_size - 100}-{total_size - 1}")
    assert r.content == r2.content


def test_range_end_is_clamped(file_url, total_size):
    r = _get_range(file_url, f"bytes={total_size - 5}-{total_size + 1000}")
    assert r.status_code == 206
    assert r.headers["Content-Range"] == (
        f"bytes {total_size - 5}-{total_size - 1}/{total_size}")
    assert len(r.content) == 5


def test_range_not_satisfiable(file_url, total_size):
    r = _get_range(file_url, f"bytes={total_size}-")
    assert r.status_code == 416
    assert r.headers["Content-Range"] == f"bytes */{total_size}"


def test_head_with_range(file_url, total_size):
    header, body = _raw_request(file_url, "HEAD", {"Range": "bytes=0-99"})
    assert header.startswith("HTTP/1.1 206"), header
    assert f"bytes 0-99/{total_size}" in header
    assert body == b""


def test_range_is_not_reused(file_url, total_size):
    """A Range header must not leak into the next request."""
    assert _get_range(file_url, "bytes=0-9").status_code == 206
    r = requests.head(file_url, timeout=TIMEOUT)
    assert r.status_code == 200
    assert int(r.headers["Content-Length"]) == total_size


def _file_url_with_id(file_url: str, id_value: str) -> str:
    u = urlparse(file_url)
    return f"{u.scheme}://{u.netloc}{u.path}?ID={id_value}"


@pytest.mark.parametrize("id_value", ["999999", "0", "abc", ""])
def test_invalid_id(file_url, id_value):
    """Unknown ids, the root directory and garbage must give 404."""
    r = requests.get(_file_url_with_id(file_url, id_value), timeout=TIMEOUT)
    assert r.status_code == 404


def test_directory_id(file_url):
    """Directories can not be downloaded: request the id of the first
    container of the root."""
    u = urlparse(file_url)
    body = (
        '<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/'
        'soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/'
        'encoding/"><s:Body><u:Browse xmlns:u="urn:schemas-upnp-org:service:'
        'ContentDirectory:1"><ObjectID>0</ObjectID><BrowseFlag>'
        'BrowseDirectChildren</BrowseFlag><Filter>*</Filter><StartingIndex>0'
        '</StartingIndex><RequestedCount>0</RequestedCount><SortCriteria>'
        '</SortCriteria></u:Browse></s:Body></s:Envelope>')
    r = requests.post(
        f"{u.scheme}://{u.netloc}/CD/control", data=body, timeout=TIMEOUT,
        headers={"Content-Type": 'text/xml; charset="utf-8"',
                 "SOAPACTION": '"urn:schemas-upnp-org:service:'
                               'ContentDirectory:1#Browse"'})
    assert r.status_code == 200
    m = re.search(r'container id=(?:"|&quot;)(\d+)', r.text)
    if not m:
        pytest.skip("no container in the root")
    r = requests.get(_file_url_with_id(file_url, m.group(1)), timeout=TIMEOUT)
    assert r.status_code == 404
