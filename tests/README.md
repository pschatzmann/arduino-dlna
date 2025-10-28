Tests for the device-media-server example

These are simple integration tests that POST SOAP actions to a running
instance of the `device-media-server` example. They use the hard-coded
address `127.0.0.1:44757`.

Quick start:

1. Build and run the `device-media-server` example (on the host or device).
2. Install the test dependencies:

```bash
python3 -m pip install -r tests/requirements.txt
```

3. Run pytest in the repository root:

```bash
pytest -q tests/test_media_server.py
```

Override the base URL

The tests use the hard-coded address `127.0.0.1:44757` by default. You can
override this by setting the `DLNA_TEST_BASE` environment variable. For
example, if your server runs at `192.168.1.50:44757`:

```bash
export DLNA_TEST_BASE=http://192.168.1.50:44757
pytest -q tests/test_media_server.py
```
Alternatively, use the pytest CLI option `--base` to override the server URL:

```bash
pytest -q tests/test_media_server.py --base=http://192.168.1.50:44757
```

Notes:
- The server must be reachable at 127.0.0.1:44757 for the tests to pass.
- These tests are intentionally simple smoke tests and assert basic XML
  structure and HTTP 200 responses. Extend them to validate full DIDL
  contents or to use a configurable base URL if you need more flexibility.
