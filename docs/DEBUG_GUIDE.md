# Debug guide

Run the six executables in separate VS2022 debugger sessions. Each loads
`config/<Service>/config.ini` when present, or the corresponding example;
`--config <path>` and `NDT_CONFIG` select another INI. Startup prints the
selected path, and failures print to stderr. Keep GateServer and AcqServer's
`[Security] JwtSecret` equal; preferably set the same `NDT_JWT_SECRET` in
both processes. Never commit a real key or database password.

| Step | Breakpoints |
| --- | --- |
| HTTP login | Gate `HttpConnection::Start`, `LogicSystem::Login`, `AuthGrpcClient::Verify`, `StatusGrpcClient::Route` |
| MySQL identity | `AuthGrpcServiceImpl::Verify`, Auth `LogicSystem::Verify`, `MysqlDao::Verify`, `MySqlPool::Acquire` |
| Route/JWT | `StatusGrpcServiceImpl::Route`, `NodeManager::Select`, `Jwt::IssueAt`, `Jwt::Verify` |
| TCP dispatch | Acq `CServer::Accept`, `CSession::ReadHeader/ReadBody`, `LogicSystem::PostMsgToQue/Dispatch` |
| Scan/FFT | `ScanTask::Run`, `DeviceBridge::Acquire`, `FftTask::Run`, `ProcGrpcClient::Fft`, `FftLogic::Calculate` |
| Archive | `FileUploadTask::Run`, `FileGrpcClient::Append`, `ArchiveLogic::Append`, `FileManager::Append/Complete`, `RedisConPool::Acquire` |
| Qt UI | `NdtWindow::onMessage`, `plotWave`, `plotSpectrum` |

Use `ctest --test-dir build-server-v144 --output-on-failure` for ConfigMgr,
JWT, FFT and protocol checks. `tests/async_io_test.py` covers many idle TCP
connections, half packets and coalesced packets. `tests/service_test.py`
covers live Status/Proc/File RPCs. `tests/full_chain_test.py` starts isolated
MySQL, Redis, simulators and all six servers, then exercises the Qt offscreen
integration executable; it writes `data/full-chain.log`. The runner keeps
passwords out of its logs. `tests/upload_recovery_test.py` writes
`data/recovery.log` and verifies restart recovery.

| Symptom | Check |
| --- | --- |
| Qt Kit warning | Qt 5.9.9 x64 qmake, matching x64 MSVC compiler and configured CDB debugger; see `ENVIRONMENT.md`. |
| HTTP 401 | User row, PBKDF2 hash, supplied password. |
| HTTP 503 | Auth/Status addresses, MySQL availability, and a fresh device-ready Acq heartbeat. |
| TCP closes on Hello | Same JWT key at Gate/Acq, token expiry, `iss`/`aud`, route node ID. |
| No route | Unique Acq node IDs, simulator ports and `Timeout.HeartbeatMs`; inspect `NodeManager::Select`. |
| Scan error | Nyquist condition, device simulator sockets and DAQ channel/sample counts. |
| FFT error | ProcServer address/deadline; run `fft_test.exe`. |
| Upload error | FileServer disk path and `.part`/`.meta`, Redis address, chunk ACK and RPC deadline. |
| Upload pending after restart | Local `<scan_id>.ready` marker and spool hash; restore FileServer and restart AcqServer. |

The original Visual Studio chat `.props` files use obsolete local paths.
Build the NDT servers with the root CMake project and VS2022 x64 toolchain.
