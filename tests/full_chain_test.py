"""Local end-to-end test: MySQL -> HTTP/gRPC -> TCP devices -> FFT/file."""
import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build-server-v144"
MYSQL_HOME = Path(r"D:\boost\mysql\mysql\mysql")
REDIS = Path(r"D:\boost\redis\Redis-x64-5.0.14.1\redis-server.exe")


def wait_port(port, process):
    for _ in range(120):
        if process.poll() is not None:
            raise RuntimeError(f"{process.args[0]} exited: {process.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise TimeoutError(f"port {port} did not open")


def send_frame(sock, kind, body, split=False):
    payload = json.dumps(body, separators=(",", ":")).encode()
    packet = struct.pack("!4sHHI", b"NDT1", 1, kind, len(payload)) + payload
    if split:
        for piece in (packet[:3], packet[3:10], packet[10:]):
            sock.sendall(piece)
            time.sleep(0.01)
    else:
        sock.sendall(packet)


def recv_exact(sock, length):
    data = bytearray()
    while len(data) < length:
        piece = sock.recv(length - len(data))
        if not piece:
            raise ConnectionError("TCP closed early")
        data.extend(piece)
    return bytes(data)


def recv_frame(sock):
    magic, version, kind, length = struct.unpack("!4sHHI", recv_exact(sock, 12))
    assert magic == b"NDT1" and version == 1 and length <= 1048576
    return kind, json.loads(recv_exact(sock, length))


def login(password):
    data = json.dumps({"username": "demo", "password": password}).encode()
    req = urllib.request.Request("http://127.0.0.1:18080/login", data=data,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, json.load(error)


def verify_jwt(token, secret, node_id, user_id):
    parts = token.split(".")
    assert len(parts) == 3
    decode = lambda part: base64.urlsafe_b64decode(part + "=" * (-len(part) % 4))
    header = json.loads(decode(parts[0]))
    claims = json.loads(decode(parts[1]))
    signature = hmac.new(secret.encode(), ".".join(parts[:2]).encode(), hashlib.sha256).digest()
    assert header == {"alg": "HS256", "typ": "JWT"}
    assert hmac.compare_digest(signature, decode(parts[2]))
    assert claims["user_id"] == user_id and claims["sub"] == str(user_id)
    assert claims["node_id"] == node_id and claims["role"] == "operator"
    assert claims["iss"] == "LaserUltrasonicServer" and claims["aud"] == "ndt-acq"
    assert claims["iat"] <= time.time() < claims["exp"] <= claims["iat"] + 300


def expect_hello_rejected(host, port, token):
    with socket.create_connection((host, port), timeout=5) as sock:
        sock.settimeout(5)
        send_frame(sock, 1, {"token": token})
        try:
            kind, reply = recv_frame(sock)
            assert kind == 8 and "JWT" in reply.get("error", ""), reply
        except ConnectionError:
            pass  # The server may close immediately after rejecting the Hello.


def create_mysql(log):
    parent = ROOT / "mysql-data"
    parent.mkdir(exist_ok=True)
    data = parent / ("run-" + os.urandom(5).hex())
    data.mkdir()
    cmd = [str(MYSQL_HOME / "bin" / "mysqld.exe"), "--no-defaults", "--initialize",
           "--console", f"--basedir={MYSQL_HOME}", f"--datadir={data}"]
    done = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=360)
    output = done.stdout + done.stderr
    if done.returncode:
        raise RuntimeError("MySQL secure initialization failed; inspect data/full-chain.log")
    match = re.search(r"temporary password is generated for root@localhost: (\S+)", output)
    if not match:
        raise RuntimeError("MySQL temporary password not found in init output")
    log.write(output.replace(match.group(1), "[REDACTED]"))
    log.flush()
    return data, match.group(1)


def mysql_exec(sql, password):
    env = os.environ.copy()
    env["MYSQL_PWD"] = password
    command = [str(MYSQL_HOME / "bin" / "mysql.exe"), "--protocol=tcp", "--connect-expired-password",
               "-h", "127.0.0.1", "-P", "13306", "-u", "root"]
    subprocess.run(command, input=sql, text=True, env=env, check=True, timeout=15,
                   stdout=subprocess.DEVNULL)


def mysql_auth_connection_ids(password):
    env = os.environ.copy()
    env["MYSQL_PWD"] = password
    output = subprocess.check_output(
        [str(MYSQL_HOME / "bin" / "mysql.exe"), "--protocol=tcp", "-N", "-B",
         "-h", "127.0.0.1", "-P", "13306", "-u", "root", "-e",
         "SELECT ID FROM information_schema.PROCESSLIST WHERE USER='root' AND DB='ndt'"],
        env=env, text=True, timeout=15)
    return sorted(output.split())


def main():
    (ROOT / "data").mkdir(exist_ok=True)
    log = (ROOT / "data" / "full-chain.log").open("w", encoding="utf-8")
    processes = []
    admin_password = None
    try:
        mysql_data, temporary_password = create_mysql(log)
        env = os.environ.copy()
        env.update(NDT_MYSQL_HOST="127.0.0.1", NDT_MYSQL_PORT="13306",
                   NDT_MYSQL_USER="root", NDT_MYSQL_DATABASE="ndt",
                   NDT_REDIS_HOST="127.0.0.1", NDT_REDIS_PORT="16379",
                   NDT_AUTH_PORT="15051", NDT_STATUS_PORT="15052", NDT_PROC_PORT="15053", NDT_FILE_PORT="15054",
                   NDT_GATE_PORT="18080", NDT_FILE_ROOT=str(ROOT / "data" / "archive"),
                   NDT_SPOOL_ROOT=str(ROOT / "spool"),
                   NDT_JWT_SECRET=secrets.token_hex(32))
        def launch(command, port, custom=None):
            proc = subprocess.Popen(command, cwd=ROOT, env=custom or env, stdout=log,
                                    stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            processes.append(proc)
            wait_port(port, proc)
            return proc

        mysql_command = [str(MYSQL_HOME / "bin" / "mysqld.exe"), "--no-defaults", "--console",
                         f"--basedir={MYSQL_HOME}", f"--datadir={mysql_data}", "--port=13306",
                         "--bind-address=127.0.0.1", "--mysqlx=OFF"]
        mysql_process = launch(mysql_command, 13306)
        admin_password = os.urandom(24).hex()
        mysql_exec(f"ALTER USER 'root'@'localhost' IDENTIFIED BY '{admin_password}';", temporary_password)
        env["NDT_MYSQL_PASSWORD"] = admin_password
        init_env = env.copy()
        init_env["NDT_MYSQL_ADMIN_PASSWORD"] = admin_password
        init_env["NDT_MYSQL_ADMIN_USER"] = "root"
        output = subprocess.check_output([sys.executable, str(ROOT / "scripts" / "init_test_user.py")],
                                         cwd=ROOT, env=init_env, text=True, timeout=15)
        test_password = output.strip().splitlines()[-1].split(": ", 1)[1]
        env["NDT_TEST_PASSWORD"] = test_password
        launch([str(REDIS), "--port", "16379", "--bind", "127.0.0.1", "--save", "",
                "--appendonly", "no"], 16379)
        launch([sys.executable, "-B", str(ROOT / "simulator" / "devices.py"), "--base-port", "19000"], 19002)
        launch([sys.executable, "-B", str(ROOT / "simulator" / "devices.py"), "--base-port", "19100"], 19102)
        launch([str(BIN / "AuthServer.exe")], 15051)
        launch([str(BIN / "StatusServer.exe")], 15052)
        launch([str(BIN / "ProcServer.exe")], 15053)
        launch([str(BIN / "FileServer.exe")], 15054)
        subprocess.run([str(BIN / "rpc_probe.exe"), "auth"], cwd=ROOT, env=env, check=True, timeout=20)
        pooled_ids = mysql_auth_connection_ids(admin_password)
        assert pooled_ids, "AuthServer did not retain an idle MySQL connection"
        subprocess.run([str(BIN / "rpc_probe.exe"), "auth"], cwd=ROOT, env=env, check=True, timeout=20)
        assert mysql_auth_connection_ids(admin_password) == pooled_ids, "MySQL connection was not reused"
        print("MySQL connection reused across Auth RPCs")
        shutdown_env = env.copy()
        shutdown_env["MYSQL_PWD"] = admin_password
        subprocess.run([str(MYSQL_HOME / "bin" / "mysqladmin.exe"), "--protocol=tcp",
                        "-h", "127.0.0.1", "-P", "13306", "-u", "root", "shutdown"],
                       env=shutdown_env, check=True, timeout=20, stdout=subprocess.DEVNULL)
        mysql_process.wait(timeout=20)
        launch(mysql_command, 13306)
        subprocess.run([str(BIN / "rpc_probe.exe"), "auth"], cwd=ROOT, env=env, check=True, timeout=20)
        print("MySQL pooled connection recovered after database restart")
        for node, port, base in (("acq1", 17001, 19000), ("acq2", 17002, 19100)):
            node_env = env.copy()
            node_env.update(NDT_ACQ_NODE_ID=node, NDT_ACQ_PORT=str(port), NDT_DEVICE_BASE_PORT=str(base))
            launch([str(BIN / "AcqServer.exe")], port, node_env)
        launch([str(BIN / "GateServer.exe")], 18080)
        time.sleep(2.5)
        status, wrong = login(test_password + "wrong")
        assert status == 401 and not wrong["ok"], wrong
        status, routed = login(test_password)
        assert status == 200 and routed["ok"] and routed["port"] in (17001, 17002), routed
        verify_jwt(routed["token"], env["NDT_JWT_SECRET"], routed["node_id"], routed["user_id"])
        tampered = routed["token"][:-2] + ("A" if routed["token"][-2] != "A" else "B") + routed["token"][-1]
        expect_hello_rejected(routed["host"], routed["port"], tampered)
        print("HTTP login and AcqServer route passed")
        with socket.create_connection((routed["host"], routed["port"]), timeout=10) as sock:
            sock.settimeout(25)
            send_frame(sock, 1, {"token": routed["token"]}, split=True)
            kind, hello = recv_frame(sock)
            assert kind == 1 and hello["state"] == "authenticated", hello
            time.sleep(2.5)  # Let the first node report its new connection count.
            status, second_route = login(test_password)
            assert status == 200 and second_route["port"] != routed["port"], second_route
            verify_jwt(second_route["token"], env["NDT_JWT_SECRET"], second_route["node_id"], second_route["user_id"])
            expect_hello_rejected(second_route["host"], second_route["port"], routed["token"])
            with socket.create_connection((second_route["host"], second_route["port"]), timeout=10) as second:
                second.settimeout(10)
                send_frame(second, 1, {"token": second_route["token"]})
                second_kind, second_hello = recv_frame(second)
                assert second_kind == 1 and second_hello["state"] == "authenticated"
            print("Two live AcqServer instances routed according to connection load")
            send_frame(sock, 2, {"sample_rate_hz": 20e6, "sample_count": 1024,
                                 "excitation_frequency_hz": 2.75e6, "scan_step_mm": 0.5,
                                 "scan_points": 2, "channel_count": 4}, split=True)
            seen = {"wave": 0, "spectrum": 0, "file": False, "done": False}
            scan_id = None
            for _ in range(100):
                kind, message = recv_frame(sock)
                if kind == 4:
                    seen["wave"] += 1
                    assert len(message["samples"]) == 1024
                    scan_id = message["scan_id"]
                elif kind == 5:
                    seen["spectrum"] += 1
                    assert abs(message["dominant_frequency_hz"] - 2.75e6) < 20e6 / 1024
                elif kind == 7 and message.get("status") == "complete":
                    seen["file"] = True
                elif kind == 6 and message.get("state") == "complete":
                    seen["done"] = True
                elif kind == 8:
                    raise RuntimeError(f"AcqServer error: {message}")
                if seen["wave"] == 8 and seen["spectrum"] == 8 and seen["file"] and seen["done"]:
                    break
            assert seen == {"wave": 8, "spectrum": 8, "file": True, "done": True}, seen
            archive = ROOT / "data" / "archive" / f"{scan_id}.ndtbin"
            spool = ROOT / "spool" / f"{scan_id}.spool"
            assert archive.exists() and spool.exists()
            assert hashlib.sha256(archive.read_bytes()).digest() == hashlib.sha256(spool.read_bytes()).digest()
            print("TCP scan, device sockets, gRPC FFT, Redis upload and SHA-256 archive passed")
        qt_bin = ROOT / "build-client-integration" / "bin"
        (qt_bin / "config.ini").write_text("[gate]\nlogin_url=http://127.0.0.1:18080/login\n", encoding="utf-8")
        qt_env = env.copy()
        qt_root = Path(os.environ.get("NDT_QT_ROOT", r"D:\qt\5.9.9\msvc2015_64"))
        qt_env["PATH"] = str(qt_root / "bin") + os.pathsep + qt_env.get("PATH", "")
        qt_env["QT_QPA_PLATFORM"] = "offscreen"
        qt_env["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(qt_root / "plugins" / "platforms")
        result = subprocess.run([str(qt_bin / "qt_integration.exe")], cwd=qt_bin,
                                env=qt_env, text=True, capture_output=True, timeout=90)
        if result.returncode:
            raise RuntimeError(f"Qt integration failed: {result.stdout}\n{result.stderr}")
        match = re.search(r"scan_id=([0-9a-f]+)", result.stdout)
        assert match, result.stdout
        qt_archive = ROOT / "data" / "archive" / f"{match.group(1)}.ndtbin"
        qt_spool = ROOT / "spool" / f"{match.group(1)}.spool"
        assert qt_archive.exists() and qt_spool.exists()
        assert hashlib.sha256(qt_archive.read_bytes()).digest() == hashlib.sha256(qt_spool.read_bytes()).digest()
        print(result.stdout.strip())
    finally:
        if admin_password:
            shutdown_env = os.environ.copy()
            shutdown_env["MYSQL_PWD"] = admin_password
            try:
                subprocess.run([str(MYSQL_HOME / "bin" / "mysqladmin.exe"), "--protocol=tcp",
                                "-h", "127.0.0.1", "-P", "13306", "-u", "root", "shutdown"],
                               env=shutdown_env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=20, check=False)
            except (OSError, subprocess.TimeoutExpired):
                pass
        for proc in reversed(processes):
            proc.terminate()
        for proc in reversed(processes):
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()


if __name__ == "__main__":
    main()
