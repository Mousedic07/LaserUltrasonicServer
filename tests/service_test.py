"""Run local independent C++ services and exercise their real gRPC/Redis links."""
import os
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
REDIS = Path(r"D:\boost\redis\Redis-x64-5.0.14.1\redis-server.exe")


def wait_port(port, process):
    for _ in range(80):
        if process.poll() is not None:
            raise RuntimeError(f"{process.args[0]} exited early: {process.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise TimeoutError(f"port {port} did not open")


def main():
    env = os.environ.copy()
    env.update(NDT_STATUS_PORT="15052", NDT_PROC_PORT="15053", NDT_FILE_PORT="15054",
               NDT_REDIS_PORT="16379", NDT_HEARTBEAT_TIMEOUT_MS="1500",
               NDT_FILE_ROOT=str(ROOT / "data" / "test"))
    (ROOT / "data" / "test").mkdir(parents=True, exist_ok=True)
    logs = (ROOT / "data" / "test" / "service_test.log").open("w", encoding="utf-8")
    processes = []
    try:
        def launch(command, port):
            proc = subprocess.Popen(command, cwd=ROOT, env=env, stdout=logs,
                                    stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append(proc)
            wait_port(port, proc)

        launch([str(REDIS), "--port", "16379", "--bind", "127.0.0.1",
                "--save", "", "--appendonly", "no"], 16379)
        launch([str(BIN / "StatusServer.exe")], 15052)
        launch([str(BIN / "ProcServer.exe")], 15053)
        launch([str(BIN / "FileServer.exe")], 15054)
        for mode in ("status", "proc", "file"):
            subprocess.run([str(BIN / "rpc_probe.exe"), mode], cwd=ROOT, env=env,
                           check=True, timeout=30)
    finally:
        for proc in reversed(processes):
            proc.terminate()
        for proc in reversed(processes):
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        logs.close()


if __name__ == "__main__":
    main()
