# llfcchat source reuse

The detailed `origin/master`, `origin/Season_2` and `f3dde67` module-by-module
audit is in [REFACTOR_AUDIT.md](REFACTOR_AUDIT.md). The upstream GPL-3.0
license remains in this repository. Source was adapted for NDT rather than
restoring the original chat routes and message IDs.

| Original idea | Running NDT implementation | Important change |
| --- | --- | --- |
| Gate `CServer`/`HttpConnection` with Beast async IO | `server/network/GateServer` | JSON `/login` enters Gate `LogicSystem`; Auth and Status RPCs use dedicated clients. |
| Chat `CServer`/`CSession`/`MsgNode` | `server/network/AcqServer` | NDT1 magic/version/length header, async exact reads and queued writes, bounded admission. |
| Chat `LogicSystem` callback map and message queue | `server/logic/AcqServer/LogicSystem` | Bounded queue, callback dispatch outside the queue mutex, scan work on separate workers, explicit stop. |
| Chat `AsioIOServicePool` | `server/pool/common/AsioIOServicePool` | Work guards and joined IO threads; idle TCP sessions do not reserve threads. |
| Chat `MySqlPool`/`MysqlDao` | `server/pool/common/MySqlPool`, `server/logic/AuthServer/MysqlDao` | Timed RAII leases and reconnect; NDT uses prepared queries and PBKDF2 instead of chat passwords. |
| Chat `RedisConPool`/`RedisMgr` | `server/pool/common/RedisConPool` and `RedisMgr` | Timed leases, PING/reconnect, upload progress operations. |
| Chat `StatusGrpcClient` and Status service adapter | `server/network/*GrpcClient*`, `*GrpcServiceImpl*` | Shared long-lived channels, deadlines, service-specific Protobuf messages. |
| ResourceServer resume concept | `server/logic/FileServer/FileManager`, `ArchiveLogic` | Binary gRPC chunks, validated upload ID, durable `.part`/`.meta`, SHA-256 before final rename. |

The Qt client retains the original `httpmgr` and `tcpmgr` asynchronous
networking style in `client/ndt_client/HttpMgr` and `TcpMgr`. Its NDT protocol
and Widgets UI are separate from the upstream chat UI. `DistLock` was not
added: this deployment has one StatusServer and no shared multi-process
heartbeat table for such a lock to protect.

The old `Ndt*Server.cpp` monoliths are absent from the CMake target list and
the working tree. Git history retains them for behavior comparison. A
single `proto/ndt.proto` replaces duplicate per-service message definitions.
