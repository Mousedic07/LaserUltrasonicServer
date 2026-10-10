"""Restarted AcqServer resumes a disk spool from FileServer's ACK offset."""
import hashlib
import os
import secrets
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
REDIS = Path(r"D:\boost\redis\Redis-x64-5.0.14.1\redis-server.exe")
id_ = secrets.token_hex(12)
spool_dir = ROOT / "data" / "recovery_spool"
archive_dir = ROOT / "data" / "recovery_archive"
spool_dir.mkdir(parents=True, exist_ok=True)
archive_dir.mkdir(parents=True, exist_ok=True)
data = os.urandom(190000)
spool = spool_dir / f"{id_}.spool"
ready = spool_dir / f"{id_}.ready"
spool.write_bytes(data)
ready.write_text(f"{len(data)} {hashlib.sha256(data).hexdigest()}\n")
env = os.environ.copy()
env.update(NDT_FILE_PORT="15054", NDT_REDIS_PORT="16379",
           NDT_FILE_ROOT=str(archive_dir), NDT_SPOOL_ROOT=str(spool_dir),
           NDT_STATUS_PORT="19997", NDT_ACQ_PORT="17001",
           NDT_DEVICE_BASE_PORT="19990", NDT_RECOVERY_ID=id_,
           NDT_RECOVERY_SPOOL=str(spool))
processes = []
with (ROOT / "data" / "recovery.log").open("w") as log:
    try:
        def launch(command, port):
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append(process)
            for _ in range(60):
                if process.poll() is not None:
                    raise RuntimeError(f"process exited: {command}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        return
                except OSError:
                    time.sleep(0.1)
            raise TimeoutError(f"port {port} did not open")

        launch([str(REDIS), "--port", "16379", "--bind", "127.0.0.1",
                "--save", "", "--appendonly", "no"], 16379)
        launch([str(BIN / "FileServer.exe")], 15054)
        subprocess.run([str(BIN / "rpc_probe.exe"), "file_seed"], cwd=ROOT,
                       env=env, check=True, timeout=20)
        launch([str(BIN / "AcqServer.exe")], 17001)
        archive = archive_dir / f"{id_}.ndtbin"
        for _ in range(80):
            if archive.exists() and not ready.exists():
                break
            time.sleep(0.1)
        assert archive.exists() and not ready.exists(), "Acq restart did not finish upload"
        assert hashlib.sha256(archive.read_bytes()).digest() == hashlib.sha256(data).digest()
        print("AcqServer restart resumed from FileServer ACK and SHA-256 matched")
    finally:
        for process in reversed(processes):
            process.terminate()
        for process in reversed(processes):
            process.wait(timeout=5)
