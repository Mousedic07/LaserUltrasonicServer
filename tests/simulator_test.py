import json
import socket
import subprocess
import sys
import time
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def request(port, commands):
    with socket.create_connection(("127.0.0.1", port), timeout=3) as sock:
        stream = sock.makefile("rwb")
        replies = []
        for command in commands:
            stream.write((json.dumps(command) + "\n").encode())
            stream.flush()
            replies.append(json.loads(stream.readline()))
        return replies


class DeviceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc = subprocess.Popen([sys.executable, str(ROOT / "simulator" / "devices.py"),
                                     "--base-port", "19000"], stdout=subprocess.DEVNULL)
        for _ in range(30):
            try:
                with socket.create_connection(("127.0.0.1", 19002), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError("simulator did not start")

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(timeout=5)

    def test_control_and_multichannel_waveforms(self):
        self.assertTrue(request(19000, [{"command": "set_frequency", "frequency_hz": 2.75e6},
                                        {"command": "trigger"}])[-1]["ok"])
        self.assertEqual(request(19001, [{"command": "move_to", "x_mm": 0.5, "y_mm": 0}])[0]["x_mm"], 0.5)
        config = {"command": "configure", "sample_rate_hz": 20e6, "sample_count": 1024,
                  "channel_count": 4, "excitation_frequency_hz": 2.75e6}
        replies = request(19002, [config, {"command": "start_acquisition"},
                                  {"command": "read_waveform", "point": 1, "x_mm": 0.5}])
        self.assertEqual(len(replies[-1]["frames"]), 4)
        self.assertEqual(len(replies[-1]["frames"][0]["samples"]), 1024)
        self.assertNotEqual(replies[-1]["frames"][0]["samples"], replies[-1]["frames"][1]["samples"])

    def test_nyquist_failure(self):
        reply = request(19002, [{"command": "configure", "sample_rate_hz": 4e6,
                                 "sample_count": 1024, "channel_count": 4,
                                 "excitation_frequency_hz": 2.75e6}])[0]
        self.assertFalse(reply["ok"])

    def test_device_disconnect_and_timeout(self):
        with socket.create_connection(("127.0.0.1", 19000), timeout=3) as sock:
            sock.sendall(b'{"command":"simulate_disconnect"}\n')
            self.assertEqual(sock.recv(1), b"")
        with socket.create_connection(("127.0.0.1", 19001), timeout=3) as sock:
            sock.settimeout(0.3)
            sock.sendall(b'{"command":"simulate_timeout"}\n')
            with self.assertRaises(socket.timeout):
                sock.recv(1)


if __name__ == "__main__":
    unittest.main()
