"""FileServer reuses Redis connections and reconnects after a Redis restart."""
import os
import socket
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
REDIS = Path(r"D:\boost\redis\Redis-x64-5.0.14.1\redis-server.exe")
env = os.environ.copy()
env.update(NDT_FILE_PORT="26054", NDT_REDIS_PORT="26379",
           NDT_FILE_ROOT=str(ROOT / "data" / "redis-reconnect"))
(ROOT / "data" / "redis-reconnect").mkdir(parents=True, exist_ok=True)


def wait_port(port, process):
    for _ in range(60):
        if process.poll() is not None:
            raise RuntimeError(f"process exited: {process.args}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=.2):
                return
        except OSError:
            time.sleep(.1)
    raise TimeoutError(port)


with (ROOT / "data" / "redis-reconnect.log").open("w") as log:
    processes = []
    try:
        def launch(args, port):
            process = subprocess.Popen(args, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append(process)
            wait_port(port, process)
            return process

        def redis_clients():
            with socket.create_connection(("127.0.0.1", 26379), timeout=2) as sock:
                stream = sock.makefile("rb")
                sock.sendall(b"*2\r\n$6\r\nCLIENT\r\n$4\r\nLIST\r\n")
                length = int(stream.readline()[1:])
                body = stream.read(length)
                return sum(line.startswith(b"id=") for line in body.splitlines())

        command = [str(REDIS), "--port", "26379", "--bind", "127.0.0.1",
                   "--save", "", "--appendonly", "no"]
        redis = launch(command, 26379)
        launch([str(BIN / "FileServer.exe")], 26054)
        subprocess.run([str(BIN / "rpc_probe.exe"), "file"], cwd=ROOT,
                       env=env, check=True, timeout=30)
        assert redis_clients() >= 2, "FileServer did not retain a Redis connection"
        redis.terminate()
        redis.wait(timeout=5)
        redis = launch(command, 26379)
        subprocess.run([str(BIN / "rpc_probe.exe"), "file"], cwd=ROOT,
                       env=env, check=True, timeout=30)
        assert redis_clients() >= 2, "FileServer did not reconnect to Redis"
        print("Redis persistent connection and reconnect passed")
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=5)
