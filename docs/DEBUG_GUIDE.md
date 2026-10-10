# Debug guide

Run each C++ service in its own VS2022 debugger session or terminal. The
service prints its listening address and connection/RPC errors to stdout or
stderr. `tests/service_test.py` writes `data/test/service_test.log`;
`tests/full_chain_test.py` writes `data/full-chain.log`, with generated MySQL
temporary password redacted. Build output is `server-build.log`.

Useful breakpoints:

- `NdtGateServer.cpp`: `handle` after HTTP parse, after Auth `Verify`, after
  Status `Route`.
- `NdtAuthServer.cpp`: `AuthLogic::Verify` before/after the prepared MySQL
  query and PBKDF2 comparison.
- `NdtStatusServer.cpp`: `Heartbeat`, `Route` candidate filtering and
  `Validate` expiry check.
- `NdtAcqServer.cpp`: `clientTask` Hello validation, `scanTask` device calls,
  `processTask` FFT RPC and `uploadTask` Query/Append/Complete.
- `NdtFileServer.cpp`: `Query`, `Append` disk flush and `Complete` SHA check.
- `NdtWindow.cpp`: `onMessage`, `plotWave`, `plotSpectrum` in Qt Creator.

Failure triage:

| Symptom | Check |
| --- | --- |
| Qt Kit yellow warning | Qt 5.9.9 qmake path, x64 compiler ABI, installed CDB debugger; see `ENVIRONMENT.md`. |
| HTTP 503 | Auth/Status process and port, MySQL availability, Acq heartbeat and device readiness. |
| HTTP 401 | User exists, enabled flag, generated local test password. |
| No Acq route | Device simulator sockets open, unique node IDs, fresh heartbeat, `NDT_HEARTBEAT_TIMEOUT_MS`. |
| TCP close on Hello | Route token expired or StatusServer restarted. |
| Scan error | Three simulator ports, Nyquist check, DAQ sample/channel counts, device socket timeout. |
| FFT error | ProcServer gRPC address/deadline; `fft_test.exe` isolates algorithm. |
| Upload error | Redis host/port, FileServer log, `.meta` total, `.part` size and RPC deadline. |
| MySQL error | Connector DLL beside AuthServer, DB user/password in environment, schema initialization. |

The original server `.props` files in `origin/master` reference obsolete
`D:\cppsoft` paths; use the NDT CMake build. Qt
Creator's saved Kit XML alone does not prove its GUI debugger works.
