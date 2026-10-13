# Server architecture

Six separate VS2022 x64 processes implement the NDT system. The Qt 5.9.9
Widgets client communicates only through HTTP and framed TCP, so its MSVC
toolset can differ from the servers'. The simulator exposes three TCP device
endpoints. MySQL stores users; Redis stores upload progress.

```text
Qt HttpMgr → GateServer → AuthServer → MySqlPool → MySQL
                       ↘ StatusServer ← AcqServer heartbeats
Qt TcpMgr  → AcqServer → simulator laser/stage/DAQ
                       → ProcServer (FFT)
                       → FileServer → disk + RedisConPool → Redis
```

Every service follows the same source boundary:

| Directory | Responsibility |
| --- | --- |
| `server/network/<service>` | HTTP/TCP accept, asynchronous session IO, gRPC clients and thin service adapters |
| `server/logic/<service>` | `LogicSystem` entry, routing, authentication, acquisition, FFT and archive rules |
| `server/pool/common` | `ConfigMgr`, `AsioIOServicePool`, MySQL and Redis pools, reusable gRPC channels, JWT |

`server/network/common/NdtFrameCodec` validates and writes NDT1 frames.
`proto/ndt.proto` is the only RPC schema; CMake generates a shared
`ndt_proto` library in `<build-directory>/generated`. Each service's `main`
loads its INI and starts its transport and logic objects.

GateServer uses Boost.Beast asynchronous HTTP. After reading a complete
request, `HttpConnection` posts it to Gate `LogicSystem`'s bounded message
queue. One independent worker thread performs login, AuthServer verification
and StatusServer routing, then posts the result back to the Asio executor for
an asynchronous HTTP response. A full queue returns HTTP 503. AuthServer's
`MysqlDao` obtains a RAII connection from `MySqlPool`, verifies the stored
PBKDF2 hash, and returns the role. StatusServer's `NodeManager` records
heartbeats and selects device-ready nodes whose heartbeat is fresh. It
minimizes `connections + 2 × running_tasks` and rotates equal scores.
GateServer signs a five-minute HS256 JWT containing the assigned node ID.

AcqServer uses `AsioIOServicePool` for TCP sessions. `CSession` performs
asynchronous exact-length reads/writes; one independent `LogicSystem`
thread consumes the bounded message queue and dispatches Hello, Start, Stop
and Status by message ID. JWT validation occurs in its Hello callback.
Each admitted scan runs on its own joinable thread, with `Scan.MaxConcurrent`
limiting active scans. Completed threads are reaped by the logic thread;
shutdown requests stop and joins every scan before its clients are destroyed.
Each scan retains a dedicated FFT worker and upload worker with bounded
queues. A full logic queue rejects a request, while full FFT/upload queues
apply backpressure to acquisition.

ProcServer's service adapter calls `FftLogic`; FileServer's adapter calls
`ArchiveLogic`, which uses `FileManager` for disk state and `RedisMgr` for
progress. `MySqlPool` and `RedisConPool` reuse checked connections, limit
their counts, and return leases through RAII. Upload ACKs follow a disk
flush. A `.ready` marker allows AcqServer to resume completed local scans
after restart; FileServer's `.part` and `.meta` files determine the confirmed
offset. `Complete` verifies SHA-256 before renaming to `.ndtbin`.

The default example INIs bind to loopback. GateServer and AcqServer share a
JWT secret through `NDT_JWT_SECRET` or their untracked `config.ini` files;
the example files contain no secret. HS256 means both processes can sign
tokens and therefore belong to one trust boundary. For deployment across
machines, protect HTTP, TCP, gRPC, MySQL and Redis connections with TLS and
restrict access to the signing key. StatusServer is a single in-memory
router; multi-instance routing would need a shared heartbeat store.
