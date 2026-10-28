#!/usr/bin/env python3
"""Run the production script in a private, headless virtual KWin session."""
import os
import json
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run():
    if not os.environ.get("KFANCYZONES_PRIVATE_TEST_BUS"):
        with tempfile.TemporaryDirectory(prefix="kfancyzones-test-bus-") as tmp:
            env = os.environ.copy()
            for key, name in [("XDG_RUNTIME_DIR", "runtime"), ("XDG_CONFIG_HOME", "config"),
                              ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"),
                              ("XDG_STATE_HOME", "state")]:
                directory = Path(tmp) / name
                directory.mkdir(mode=0o700)
                env[key] = str(directory)
            env.update(KFANCYZONES_PRIVATE_TEST_BUS="1", QT_NO_XDG_DESKTOP_PORTAL="1",
                       QT_ACCESSIBILITY="0", NO_AT_BRIDGE="1", QT_QPA_PLATFORMTHEME="generic")
            with (ROOT / "build/integration-bus.log").open("w") as log:
                process = subprocess.Popen(["dbus-run-session", "--", sys.executable, __file__, "--private"],
                                           env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    result = process.wait(timeout=40)
                except subprocess.TimeoutExpired:
                    result = 1
                finally:
                    try:
                        os.killpg(process.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    process.wait(timeout=5)
            print((ROOT / "build/integration-bus.log").read_text()[-12000:])
            return result

    with tempfile.TemporaryDirectory(prefix="kfancyzones-integration-") as tmp:
        base = Path(tmp)
        env = os.environ.copy()
        for key, name in [("XDG_RUNTIME_DIR", "runtime"), ("XDG_CONFIG_HOME", "config"),
                          ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"),
                          ("XDG_STATE_HOME", "state")]:
            directory = base / name
            directory.mkdir(mode=0o700)
            env[key] = str(directory)
        env.update(KWIN_COMPOSE="Q", KFANCYZONES_INTEGRATION_TEST="1", QT_QUICK_BACKEND="software")
        for key in ["WAYLAND_DISPLAY", "DISPLAY", "QT_QPA_PLATFORM", "SESSION_MANAGER"]:
            env.pop(key, None)

        package = base / "data/kwin/scripts/kfancyzones"
        shutil.copytree(ROOT / "src", package)
        testnative = package / "contents/ui/testnative"
        shutil.copytree(ROOT / "build/native/testqml", testnative)
        entry = json.loads((package / "metadata.json").read_text())["X-Plasma-MainScript"]
        main = package / "contents" / entry
        text = main.read_text().replace('import "moveinput" as Native',
                                       'import "moveinput" as Native\nimport "testnative" as TestNative')
        subdivide = os.environ.get("KFANCYZONES_TEST_SUBDIVIDE") == "1"
        fixture = (ROOT / "tests/integration.qml.inc").read_text()
        if subdivide:
            fixture = fixture.replace("integrationSubdivide: false", "integrationSubdivide: true")
        text = text[:text.rfind("}")] + fixture + "\n}\n"
        main.write_text(text)
        # KWin's package structure can still select ui/main.qml regardless of
        # X-Plasma-MainScript; instrument the canonical entry point as well.
        (package / "contents/ui/main.qml").write_text(text)
        (base / "config/kwinrc").write_text("[Plugins]\nkfancyzonesEnabled=true\n"
                                           "[Script-kfancyzones]\n"
                                           "enableDebugLogging=true\n"
                                           + ("subdivideZones=true\n" if subdivide else ""))
        client = ROOT / "build/native/kfancyzones-test-client"
        command = ["kwin_wayland", "--virtual", "--width", "1920", "--height", "1080",
                   "--output-count", "2", "--no-lockscreen", "--no-global-shortcuts", "--no-kactivities", str(client)]
        log_path = ROOT / "build/integration.log"
        with log_path.open("w") as log:
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 25
                reloaded = False
                while time.monotonic() < deadline:
                    output = log_path.read_text()
                    passes = output.count("KFANCYZONES_INTEGRATION: PASS")
                    if passes and not reloaded:
                        # Reload in the same engine to catch stale window callbacks.
                        subprocess.run(["qdbus6", "org.kde.KWin", "/Scripting",
                                        "org.kde.kwin.Scripting.unloadScript", "kfancyzones"],
                                       env=env, check=True, stdout=subprocess.DEVNULL)
                        subprocess.run(["qdbus6", "org.kde.KWin", "/Scripting",
                                        "org.kde.kwin.Scripting.start"],
                                       env=env, check=True, stdout=subprocess.DEVNULL)
                        reloaded = True
                    if passes >= 2:
                        print(f"Virtual KWin integration: PASS ({'8x4' if subdivide else '4x2'}, two outputs, Ctrl+Shift, bounding preview, "
                              "mouse-first acceptance, Shift-first cancellation, detachment, Ctrl release, Escape, hot reload)")
                        return 0
                    if ("KFANCYZONES_INTEGRATION: FAIL" in output or "Component failed to load" in output
                            or "TypeError:" in output or "ReferenceError:" in output
                            or process.poll() is not None):
                        break
                    time.sleep(0.1)
                print(output[-12000:], file=sys.stderr)
                print(f"Integration failed; log: {log_path}", file=sys.stderr)
                return 1
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    raise SystemExit(run())
