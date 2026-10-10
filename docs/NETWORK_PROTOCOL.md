# Network protocol

## Qt ↔ GateServer

`POST /login`, `Content-Type: application/json`, body
`{"username":"...","password":"..."}`. Success returns `ok`, `user_id`,
`role`, `host`, `port`, `node_id`, and five-minute `token`. Bad credentials
return HTTP 401; unavailable Auth/Status RPC returns HTTP 503. The client uses
an asynchronous five-second HTTP timeout.

## Qt ↔ AcqServer

Every TCP frame has a 12-byte header followed by UTF-8 JSON:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII `NDT1` magic |
| 4 | 2 | protocol version `1`, big-endian |
| 6 | 2 | message type, big-endian |
| 8 | 4 | payload length, big-endian, at most 1 MiB |

Types: 1 Hello, 2 Start, 3 Stop, 4 Wave, 5 Spectrum, 6 Status, 7 File,
8 Error. Unknown versions and oversized or invalid JSON frames close the
connection. Both the Qt buffer parser and the server's exact-length reads
support fragmented and coalesced TCP packets. A Hello with the route token
must be the first frame. JSON `Wave` includes scan/frame/channel IDs,
timestamp, position in mm, sample rate in Hz and float samples. `Spectrum`
includes frequency bins in Hz, magnitudes and dominant frequency in Hz.

## AcqServer ↔ simulated devices

Three persistent TCP connections use one compact JSON object per newline.
Laser commands: `connect`, `set_frequency`, `trigger`, `stop`, `get_status`.
Stage commands: `move_to`, `set_step`, `get_position`, `stop`. DAQ commands:
`configure`, `start_acquisition`, `read_waveform`, `stop_acquisition`.
`simulate_disconnect` and `simulate_timeout` test faults. A response has
`ok` and either result fields or `error`.

## C++ service RPCs

All services use the single [Protobuf schema](../proto/ndt.proto):
`AuthService.Verify`, `StatusService.Heartbeat/Route/Validate`,
`ProcessService.Fft`, `ArchiveService.Query/Append/Complete`. Calls use
deadlines, typically three seconds, five seconds for FFT/chunk append and
30 seconds for final SHA-256 verification. Archive chunks are at most
65,536 bytes. `Query` returns the disk-confirmed offset. A mismatched offset
returns `offset_mismatch`, prompting re-query.
On restart AcqServer scans its local `<scan_id>.ready` markers, verifies the
matching spool's size and SHA-256, and calls `Query`/`Append`/`Complete` from
the acknowledged offset. It removes a marker only after `Complete` succeeds.

## Raw archive layout

Each channel frame is one little-endian `WAV1` record: magic (4), frame ID
(uint64), channel ID (uint32), Unix timestamp ms (int64), X and Y mm
(float64 each), sample rate Hz (float64), sample count (uint32), then that
many IEEE-754 float32 samples. Record size is `52 + 4 × sample_count` bytes.
The upload ID is the scan ID and determines the server-side filename; user
supplied path segments are rejected. Files are limited to 4 GiB in this demo.
