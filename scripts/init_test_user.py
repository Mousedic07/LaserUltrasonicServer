"""Initialize the local demo schema and create a fresh random test password."""
import hashlib
import os
import secrets
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MYSQL = os.environ.get("NDT_MYSQL_CLI", r"D:\boost\mysql\mysql\mysql\bin\mysql.exe")


def main():
    host = os.environ.get("NDT_MYSQL_HOST", "127.0.0.1")
    port = os.environ.get("NDT_MYSQL_PORT", "3306")
    user = os.environ.get("NDT_MYSQL_ADMIN_USER", "root")
    command = [MYSQL, "--protocol=tcp", "-h", host, "-P", port, "-u", user]
    env = os.environ.copy()
    if "NDT_MYSQL_ADMIN_PASSWORD" in env:
        env["MYSQL_PWD"] = env["NDT_MYSQL_ADMIN_PASSWORD"]
    schema = (ROOT / "sql" / "schema.sql").read_text(encoding="utf-8")
    subprocess.run(command, input=schema, text=True, env=env, check=True)
    password = secrets.token_urlsafe(18)
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, 200000)
    statement = ("USE ndt; INSERT INTO users (username, role, salt_hex, password_hash_hex, enabled) "
                 f"VALUES ('demo', 'operator', '{salt.hex()}', '{digest.hex()}', TRUE) "
                 "ON DUPLICATE KEY UPDATE role=VALUES(role), salt_hex=VALUES(salt_hex), "
                 "password_hash_hex=VALUES(password_hash_hex), enabled=VALUES(enabled);")
    subprocess.run(command, input=statement, text=True, env=env, check=True)
    print("Local test account: demo")
    print("Local test password (shown once):", password)


if __name__ == "__main__":
    main()
