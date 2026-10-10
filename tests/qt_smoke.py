"""Check that the Qt 5.9.9 Widgets executable starts with its runtime DLLs."""
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QT = Path(r"D:\qt\5.9.9\msvc2015_64")
EXE = ROOT / "build-client" / "bin" / "ndt_client.exe"

env = os.environ.copy()
env["PATH"] = str(QT / "bin") + os.pathsep + env.get("PATH", "")
env["QT_QPA_PLATFORM"] = "offscreen"
env["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(QT / "plugins" / "platforms")
process = subprocess.Popen([str(EXE)], cwd=EXE.parent, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           creationflags=subprocess.CREATE_NO_WINDOW)
try:
    time.sleep(2)
    if process.poll() is not None:
        out, err = process.communicate(timeout=1)
        raise RuntimeError(f"Qt client exited {process.returncode}: {err.decode(errors='replace')}")
    print("Qt 5.9.9 Widgets client started with offscreen platform")
finally:
    process.terminate()
    process.wait(timeout=5)
