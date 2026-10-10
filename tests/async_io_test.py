"""Idle TCP sessions must leave AcqServer's IO threads free for new frames."""
import json
import os
import secrets
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
env = os.environ.copy()
env.update(NDT_STATUS_PORT="25052", NDT_ACQ_PORT="27001",
           NDT_DEVICE_BASE_PORT="29000", NDT_ACQ_NODE_ID="async-test",
           NDT_SPOOL_ROOT=str(ROOT / "data" / "async-spool"),
           NDT_JWT_SECRET=secrets.token_hex(32))
processes = []
sockets = []
(ROOT / "data").mkdir(exist_ok=True)
with (ROOT / "data" / "async-io.log").open("w") as log:
    try:
        def launch(args, port):
            process = subprocess.Popen(args, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append(process)
            for _ in range(70):
                if process.poll() is not None:
                    raise RuntimeError(f"process exited: {args}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=.2):
                        return
                except OSError:
                    time.sleep(.1)
            raise TimeoutError(f"port {port} did not open")

        launch([sys.executable, "-B", str(ROOT / "simulator" / "devices.py"),
                "--base-port", "29000"], 29002)
        launch([str(BIN / "StatusServer.exe")], 25052)
        launch([str(BIN / "AcqServer.exe")], 27001)
        for _ in range(24):
            connection = socket.create_connection(("127.0.0.1", 27001), timeout=2)
            connection.settimeout(.15)
            sockets.append(connection)
        time.sleep(.5)
        for connection in sockets:
            try:
                assert connection.recv(1) != b"", "idle session was rejected"
            except socket.timeout:
                pass
        active = socket.create_connection(("127.0.0.1", 27001), timeout=2)
        active.settimeout(5)
        sockets.append(active)
        body = json.dumps({"token": "invalid"}).encode()
        active.sendall(b"NDT1" + struct.pack("!HHI", 1, 1, len(body)) + body)
        assert active.recv(1024) == b"", "new session was not processed while others idled"
        print("24 idle sessions stayed open and a 25th frame was processed")
    finally:
        for connection in sockets:
            connection.close()
        for process in reversed(processes):
            process.terminate()
        for process in reversed(processes):
            process.wait(timeout=5)
