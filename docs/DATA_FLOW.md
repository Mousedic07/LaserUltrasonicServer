# One waveform's data flow

1. Qt sends `POST /login` to GateServer. AuthServer checks the user in MySQL.
   StatusServer selects a live, device-ready AcqServer and issues a token.
2. Qt opens TCP to that node and sends an NDT1 `Hello` frame with the token.
   AcqServer verifies it through StatusServer's `Validate` RPC.
3. Qt sends `Start` with sample rate, count, excitation, step, points and
   channels. AcqServer rejects a configuration violating Nyquist or file limits.
4. AcqServer sends `set_frequency` and `configure` over real TCP sockets to
   laser and DAQ. For each position it sends `move_to`, `trigger` and
   `read_waveform`. The seeded simulator returns channel-specific samples.
5. For each channel, AcqServer writes a fixed-layout record to its spool and
   sends a `Wave` frame to Qt. It submits the samples to an eight-item FFT
   queue and fixed-size file chunks to an eight-item archive queue.
6. The FFT worker calls ProcServer with one Protobuf `Waveform`. ProcServer
   returns frequency bins, one-sided normalized magnitudes and a measured
   dominant frequency. AcqServer forwards a `Spectrum` frame to Qt.
7. The archive worker queries FileServer's persisted offset, uploads chunks
   by gRPC `Append` and forwards progress/speed to Qt. FileServer flushes the
   `.part` file before acknowledging each offset. Redis stores progress and
   speed in `ndt:upload:{upload_id}`. The disk `.meta` file and `.part` file
   remain authoritative if Redis state is lost.
8. When all points are recorded, AcqServer hashes its spool and calls
   `Complete`. FileServer checks total size and SHA-256, atomically renames
   `.part` to `.ndtbin`, and reports completion. Qt shows the task result.

If the FileServer connection drops mid-upload, AcqServer re-queries the
server-confirmed offset and retries. A stopped scan leaves a partial spool and
reports partial status; it is never marked as a completed archive.
