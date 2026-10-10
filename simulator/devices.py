"""Three real TCP device simulators. Run two copies with different base ports for two nodes."""
import argparse
import json
import math
import random
import socketserver
import threading
import time


class LaserSimulator:
    def __init__(self):
        self.frequency_hz = 2.75e6
        self.connected = True

    def handle(self, req):
        command = req.get("command")
        if command == "set_frequency":
            value = float(req.get("frequency_hz", 0))
            if value <= 0:
                raise ValueError("frequency_hz must be positive")
            self.frequency_hz = value
        elif command == "trigger":
            if not self.connected:
                raise ValueError("laser stopped")
        elif command == "stop":
            self.connected = False
        elif command == "connect":
            self.connected = True
        elif command != "get_status":
            raise ValueError("unknown laser command")
        return {"ok": True, "connected": self.connected, "frequency_hz": self.frequency_hz}


class StageSimulator:
    def __init__(self):
        self.x_mm = 0.0
        self.y_mm = 0.0
        self.step_mm = 0.5

    def handle(self, req):
        command = req.get("command")
        if command == "move_to":
            self.x_mm = float(req.get("x_mm", 0))
            self.y_mm = float(req.get("y_mm", 0))
        elif command == "set_step":
            self.step_mm = float(req.get("step_mm", 0.5))
            if self.step_mm <= 0:
                raise ValueError("step_mm must be positive")
        elif command in ("stop", "get_position"):
            pass
        else:
            raise ValueError("unknown stage command")
        return {"ok": True, "x_mm": self.x_mm, "y_mm": self.y_mm, "step_mm": self.step_mm}


class DAQSimulator:
    def __init__(self):
        self.sample_rate_hz = 20e6
        self.sample_count = 1024
        self.channel_count = 4
        self.excitation_frequency_hz = 2.75e6
        self.started = False

    def handle(self, req):
        command = req.get("command")
        if command == "configure":
            rate = float(req.get("sample_rate_hz", 0))
            count = int(req.get("sample_count", 0))
            channels = int(req.get("channel_count", 0))
            excitation = float(req.get("excitation_frequency_hz", 0))
            if not (rate > 2 * excitation > 0 and 64 <= count <= 16384 and 1 <= channels <= 16):
                raise ValueError("invalid DAQ configuration or Nyquist violation")
            self.sample_rate_hz, self.sample_count = rate, count
            self.channel_count, self.excitation_frequency_hz = channels, excitation
        elif command == "start_acquisition":
            self.started = True
        elif command == "stop_acquisition":
            self.started = False
        elif command == "read_waveform":
            if not self.started:
                raise ValueError("DAQ not started")
            point = int(req.get("point", 0))
            x_mm = float(req.get("x_mm", 0))
            y_mm = float(req.get("y_mm", 0))
            frames = []
            for channel in range(self.channel_count):
                rng = random.Random(20261010 + point * 31 + channel)
                amplitude = 1.0 - channel * 0.11
                phase = channel * 0.23
                samples = []
                for i in range(self.sample_count):
                    t = i / self.sample_rate_hz
                    value = (amplitude * math.sin(2 * math.pi * self.excitation_frequency_hz * t + phase)
                             + 0.14 * math.sin(2 * math.pi * 1.1e6 * t)
                             + rng.gauss(0, 0.015))
                    samples.append(round(value, 6))
                frames.append({"channel_id": channel, "timestamp_ms": int(time.time() * 1000),
                               "position_x_mm": x_mm, "position_y_mm": y_mm,
                               "sample_rate_hz": self.sample_rate_hz, "samples": samples})
            return {"ok": True, "frames": frames}
        else:
            raise ValueError("unknown DAQ command")
        return {"ok": True, "started": self.started}


class DeviceHandler(socketserver.StreamRequestHandler):
    def handle(self):
        device = self.server.device_factory()
        self.request.settimeout(10)
        for raw in self.rfile:
            if len(raw) > 16 * 1024 * 1024:
                break
            try:
                req = json.loads(raw)
                if req.get("command") == "simulate_disconnect":
                    return
                if req.get("command") == "simulate_timeout":
                    time.sleep(15)
                    return
                reply = device.handle(req)
            except (ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
                reply = {"ok": False, "error": str(exc)}
            self.wfile.write((json.dumps(reply, separators=(",", ":")) + "\n").encode())


class DeviceServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-port", type=int, default=9000,
                        help="laser=base, stage=base+1, DAQ=base+2")
    parser.add_argument("--host", default="127.0.0.1")
    args = parser.parse_args()
    servers = []
    for offset, factory in enumerate((LaserSimulator, StageSimulator, DAQSimulator)):
        server = DeviceServer((args.host, args.base_port + offset), DeviceHandler)
        server.device_factory = factory
        servers.append(server)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        print(f"{factory.__name__} listening on {args.host}:{args.base_port + offset}", flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        for server in servers:
            server.shutdown()


if __name__ == "__main__":
    main()
