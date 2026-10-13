# Source call walkthrough

These paths are the actual runtime calls. Class names are qualified by
service namespaces such as `ndtapp::gate` and `ndtapp::acq`, so the six
`LogicSystem` classes remain separate.

## Login and node route

1. `client/ndt_client/NdtWindow::buildLogin` connects the Login button to
   `HttpMgr::login`, which uses Qt's asynchronous `QNetworkAccessManager`.
2. Gate `CServer::Start` accepts HTTP sockets. `HttpConnection::Start` uses
   Beast `async_read` for the complete request, then
   `LogicSystem::PostMsgToQue` enqueues it in a bounded queue. Gate
   `LogicSystem::DealMsg` runs on one independent worker thread, calls
   `Handle` and `Login`, and posts the result back to the IO executor.
   `HttpConnection::Respond` performs an asynchronous HTTP write.
3. `AuthGrpcClient::Verify` sends `ndt::LoginRequest` to
   `AuthGrpcServiceImpl::Verify`. Auth `LogicSystem::Verify` calls
   `MysqlDao::Verify`, which obtains `MySqlPool::Handle`. The prepared
   query reads the user hash; PBKDF2 verification returns user ID and role.
   The handle automatically returns the checked connection to the pool.
4. Gate `StatusGrpcClient::Route` sends the user ID to
   `StatusGrpcServiceImpl::Route`. Status `LogicSystem::Route` calls
   `NodeManager::Select`. Acq's `LogicSystem::HeartbeatLoop` reports live
   connections, active scans and device readiness through its
   `StatusGrpcClient::Heartbeat`. The manager rejects stale/unready nodes,
   scores the rest by `connections + 2 × running_tasks`, and rotates ties.
5. Gate `Jwt::Issue` signs the selected `node_id`, user, role, issuer,
   audience, issue time and expiry. `HttpConnection::Respond` returns the
   route and JWT to `HttpMgr`, whose signal updates `NdtWindow`.

## TCP session and scan

1. `NdtWindow` calls `TcpMgr::connectTo` with the route. Qt's `QTcpSocket`
   sends a framed Hello carrying the JWT on connect.
2. Acq `CServer::Accept` assigns the socket to an `AsioIOServicePool`
   context. `CSession::ReadHeader` and `ReadBody` use `boost::asio::async_read`
   to get the complete 12-byte header and bounded JSON body. The
   `NdtFrameCodec` validates magic, version and length, then constructs a
   `MsgNode`.
3. `LogicSystem::PostMsgToQue` enqueues the node with a capacity check.
   Its dispatch thread runs the callback registered by
   `LogicSystem::RegisterCallBacks`. Hello calls `Jwt::Verify`, checking
   HS256 signature, issuer, audience, valid time and this Acq node ID.
   A valid Hello authenticates the `CSession`.
4. A Start frame goes through the same queue. `ScanLogic::Parse` checks
   sample rate, Nyquist condition, size and scan parameters. The logic
   thread admits at most `Scan.MaxConcurrent` scans, then creates a joinable
   thread for each `ScanTask::Run`. It reaps finished scan threads and
   requests stop and joins the rest during shutdown. Device and RPC work stay
   off the Asio read callback and logic queue worker. Stop sets the session
   stop flag; Status probes the devices and sends a status frame.
5. `ScanTask::Run` configures `DeviceBridge`, which sends line-delimited
   JSON commands to the Python laser, stage and DAQ simulators. For each
   point and channel it writes a `WAV1` record to the local spool and calls
   `CSession::Send` for the Wave frame. `CSession::WriteNext` drains its
   bounded asynchronous write queue. Qt `TcpMgr::parseBuffer` handles
   fragmented/coalesced NDT1 frames and emits `message`; `NdtWindow::onMessage`
   calls `plotWave` for the selected channel.

## FFT and archive

1. `ScanTask` starts one FFT thread and one file upload thread for its
   scan. Both consume bounded queues and close their queue on exit so a
   failed worker cannot leave acquisition blocked. `ScanTask` pushes
   each Protobuf `Waveform` into the bounded FFT queue.
   `FftTask::Run` calls `ProcGrpcClient::Fft` using a reused gRPC channel and
   deadline. `ProcGrpcServiceImpl::Fft` delegates to Proc
   `LogicSystem::Process`, then `FftLogic::Calculate`. The Spectrum returns
   over gRPC; `FftTask` sends an NDT1 Spectrum frame. Qt's `plotSpectrum`
   updates the FFT chart and dominant frequency.
2. `ScanTask` pushes binary spool chunks into the bounded file queue.
   `FileUploadTask::Run` calls `FileGrpcClient::Query` for the confirmed
   offset, then `Append` for each chunk. `FileGrpcServiceImpl` delegates to
   File `LogicSystem`, `ArchiveLogic`, and `FileManager`. `FileManager`
   validates the stored `.meta` size, appends at the exact offset to
   `.part`, flushes it to disk, and acknowledges the resulting byte count.
   `ArchiveLogic` calls `RedisMgr::Progress`, which leases a persistent
   Redis connection from `RedisConPool`; the client gets File progress frames.
3. When all scan points are saved, `ScanTask` writes a `.ready` marker
   containing spool size and SHA-256. `FileUploadTask` sends `Complete` with
   the spool digest. `FileManager::Complete` verifies size and SHA-256 and renames
   `.part` to `.ndtbin`; Qt shows 100% and the archive filename.
4. On Acq restart, `LogicSystem::Start` launches
   `FileUploadTask::Recover`. It checks every `.ready` marker against the
   spool hash, asks FileServer for the disk-confirmed offset, appends the
   remaining bytes, calls `Complete`, and removes the marker only after
   success. Redis progress is useful for display; FileServer disk files are
   the authoritative resume state.

The CMake target list points to these layered sources; none of the former
single-file `Ndt*Server.cpp` implementations is linked.
