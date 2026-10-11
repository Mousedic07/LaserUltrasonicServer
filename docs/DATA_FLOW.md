# One waveform's data flow

1. Qt sends `POST /login` to GateServer. `AuthGrpcClient` calls AuthServer,
   whose `MysqlDao` leases a pooled MySQL connection and verifies PBKDF2.
   `StatusGrpcClient` obtains a live, device-ready AcqServer route, then
   GateServer signs a node-bound HS256 JWT.
2. Qt opens TCP to that node and sends an NDT1 `Hello` frame with the JWT.
   `CSession` posts the parsed `MsgNode` to AcqServer `LogicSystem`, which
   verifies signature, issuer, audience, time and assigned node locally.
3. Qt sends `Start` with sample rate, count, excitation, step, points and
   channels. AcqServer rejects a configuration violating Nyquist or file limits.
4. A bounded logic worker starts `ScanTask`; `DeviceBridge` sends
   `set_frequency` and `configure` over real TCP sockets to
   laser and DAQ. For each position it sends `move_to`, `trigger` and
   `read_waveform`. The seeded simulator returns channel-specific samples.
5. For each channel, AcqServer writes a fixed-layout record to its spool and
   sends a `Wave` frame to Qt. It submits the samples to an eight-item FFT
   queue and fixed-size file chunks to an eight-item archive queue.
6. `FftTask` calls `ProcGrpcClient` with one Protobuf `Waveform`. ProcServer
   returns frequency bins, one-sided normalized magnitudes and a measured
   dominant frequency. AcqServer forwards a `Spectrum` frame to Qt.
7. `FileUploadTask` uses `FileGrpcClient` to query FileServer's persisted offset, uploads chunks
   by gRPC `Append` and forwards progress/speed to Qt. FileServer flushes the
   `.part` file before acknowledging each offset. Redis stores progress and
   speed in `ndt:upload:{upload_id}`. The disk `.meta` file and `.part` file
   remain authoritative if Redis state is lost.
8. When all points are recorded, AcqServer hashes its spool and calls
   `Complete`. FileServer checks total size and SHA-256, atomically renames
   `.part` to `.ndtbin`, and reports completion. Qt shows the task result.

If the FileServer connection drops mid-upload, AcqServer re-queries the
server-confirmed offset and retries. After a complete scan, AcqServer writes
`<scan_id>.ready` beside the spool with its size and SHA-256. If AcqServer
restarts before upload completion, it checks this marker, verifies the spool,
queries FileServer's confirmed offset and resumes from that byte. It removes
the marker only after FileServer confirms the final hash. Recovery runs once
on startup, so restart AcqServer again after restoring an unavailable
FileServer. A stopped scan leaves a partial spool without a ready marker and
is never marked as a completed archive.
