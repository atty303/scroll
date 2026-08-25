import os
from pathlib import Path
import subprocess
import time

import pytest

from test_utils import ScrollCompositorFactory, find_node_by_title_contains


@pytest.mark.parametrize("animations_enabled", [False, True])
def test_xwayland_rapid_remap_does_not_leave_stale_view(
    scroll_compositor_factory: ScrollCompositorFactory,
    animations_enabled: bool,
) -> None:
    config = (
        "workspace 1\n"
        "xwayland force\n"
        f"animations enabled {'yes' if animations_enabled else 'no'}\n"
    )
    with scroll_compositor_factory(config) as compositor:
        display = compositor.getenv("DISPLAY")
        if not display:
            pytest.skip("Xwayland is not enabled (no DISPLAY env var in compositor)")

        compositor.wait_for_log_pattern("Xserver is ready", from_start=True)
        client_path = Path("./build/tests/x11-test-client").resolve()
        if not client_path.exists():
            pytest.skip("X11 test client not built")

        env = os.environ.copy()
        env["DISPLAY"] = display
        xauthority = compositor.getenv("XAUTHORITY")
        if xauthority:
            env["XAUTHORITY"] = xauthority

        proc = subprocess.run(
            [
                str(client_path),
                "Rapid Remap",
                "rapid_remap",
                "RapidRemap",
                "rapid-remap",
            ],
            env=env,
            timeout=10,
            check=False,
        )
        assert proc.returncode == 0

        deadline = time.monotonic() + 5
        while find_node_by_title_contains(compositor.get_tree(), "Rapid Remap"):
            if time.monotonic() >= deadline:
                raise AssertionError("rapid-remap view was not destroyed")
            time.sleep(0.01)

        compositor.cmd("workspace 2")
        if animations_enabled:
            time.sleep(0.5)
        else:
            compositor.wait_for_idle()
        assert compositor.proc.poll() is None
