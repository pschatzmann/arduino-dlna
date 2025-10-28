import os
import pytest


def pytest_addoption(parser):
    """Add --base option to pytest to specify the server base URL."""
    default = os.environ.get("DLNA_TEST_BASE", "http://127.0.0.1:44757")
    parser.addoption(
        "--base",
        action="store",
        default=default,
        help="Base URL for the running device-media-server (default from DLNA_TEST_BASE or http://127.0.0.1:44757)",
    )


@pytest.fixture
def base_url(request):
    """Fixture that provides the base URL configured via --base."""
    return request.config.getoption("--base")


def pytest_configure(config):
    """Enable live logging and show stdout/stderr from tests by default.

    This makes prints and debug output visible when running pytest without
    requiring -s. If you prefer the default pytest capture behavior, run
    pytest with -q or set the DLNA_TEST_CAPTURE env var to 'yes'.
    """
    # Enable live logging for INFO level
    try:
        config.option.log_cli = True
        config.option.log_cli_level = "INFO"
    except Exception:
        pass

    # Show captured stdout/stderr by disabling capture unless DLNA_TEST_CAPTURE
    # is explicitly set to 'yes'. This ensures print() outputs are visible.
    if os.environ.get("DLNA_TEST_CAPTURE", "no").lower() != "yes":
        try:
            config.option.capture = "no"
        except Exception:
            pass

