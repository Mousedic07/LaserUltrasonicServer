"""An absent processing process must fail within the configured RPC deadline."""
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
env = os.environ.copy()
env["NDT_PROC_PORT"] = "19998"
subprocess.run([str(ROOT / "build-server-v144" / "rpc_probe.exe"), "proc_failure"],
               cwd=ROOT, env=env, check=True, timeout=6)
