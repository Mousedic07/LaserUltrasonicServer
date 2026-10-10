"""Start AuthServer against a closed MySQL port and assert explicit RPC failure."""
import os
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
env = os.environ.copy()
env.update(NDT_AUTH_PORT="15551", NDT_MYSQL_HOST="127.0.0.1",
           NDT_MYSQL_PORT="19999", NDT_MYSQL_USER="ndt", NDT_MYSQL_PASSWORD="unused")
(ROOT / "data").mkdir(exist_ok=True)
with (ROOT / "data" / "auth-db-failure.log").open("w") as log:
    process = subprocess.Popen([str(BIN / "AuthServer.exe")], cwd=ROOT, env=env,
                               stdout=log, stderr=subprocess.STDOUT,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        for _ in range(50):
            try:
                with socket.create_connection(("127.0.0.1", 15551), timeout=0.2):
                    break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError("AuthServer did not start")
        subprocess.run([str(BIN / "rpc_probe.exe"), "auth_db_failure"], env=env,
                       cwd=ROOT, check=True, timeout=10)
    finally:
        process.terminate()
        process.wait(timeout=5)
