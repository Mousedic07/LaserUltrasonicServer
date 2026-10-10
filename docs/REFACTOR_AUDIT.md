# Source-level refactor audit

Baseline: `laser/main` at `ccdb89e`. This branch is
`codex/architecture-refactor`. The original chat sources were inspected from
`f3dde67^`, `origin/master`, and `origin/Season_2`; they remain recoverable
from Git history. No upstream chat business files are restored into the NDT
runtime merely to satisfy a class name.

| Original module | Assessment | NDT adaptation |
| --- | --- | --- |
| Gate `CServer`, `HttpConnection` | Adapt | Keep Beast asynchronous accept/read/write; move routing to Gate `LogicSystem`. Original route handlers and global singleton are chat-specific. |
| Gate `LogicSystem`, `StatusGrpcClient` | Adapt | Preserve route table and RPC wrapper roles. Replace registration/chat routes with login, Auth/Status clients, and JWT issue. |
| Gate/Chat `ConfigMgr` | Fix and adapt | Original reads only working-directory `config.ini`, prints every value including secrets, and silently returns empty strings. Add typed defaults, path selection, validation, and secret-safe diagnostics. |
| Chat `CServer`, `CSession`, `MsgNode` | Adapt | Preserve async accept, exact-length reads, per-session send queue and frame node. Replace two-short chat header and chat IDs with versioned NDT1 frames. Enforce queue bounds and safe close. |
| Chat `LogicSystem` | Fix and adapt | Its queue is unbounded and `DealMsg` calls handlers while holding its mutex, serializing producers and risking deadlock. Add bounded dispatch, worker isolation and explicit shutdown. |
| Chat `AsioIOServicePool` | Fix and migrate | Existing NDT pool already adapts this class; use it for asynchronous Acq sessions, with work guards and joined workers. |
| Chat `MysqlDao`, `MysqlMgr`, `MySqlPool` | Adapt | Keep prepared query and RAII pool concept. Original pool creates all connections eagerly and detaches a health thread. Add bounded timed checkout, reconnection and safe shutdown; retain NDT PBKDF2 schema. |
| Chat `RedisMgr`, `RedisConPool` | Adapt | Original uses hiredis and eager AUTH, including unsafe null-reply paths. Provide a bounded RAII connection pool for FileServer's RESP operations, with reconnect and no secret logging. |
| Chat `StatusGrpcClient`, `ChatGrpcClient`, `ChatConPool` | Adapt | Preserve RPC client boundaries; replace chat messages and redundant stub pools with shared long-lived gRPC channels, deadlines and explicit errors. |
| Status `StatusServiceImpl` | Adapt | The original static chat-node selection bypasses its commented load comparison. Move NDT heartbeat and load scoring into `NodeManager`; gRPC service becomes an adapter. |
| Status/Chat `DistLock` | Do not migrate | `Season_2` uses expiring `SET NX EX` and owner-checking Lua release. One StatusServer owns the current in-memory node table, so a distributed lock has no shared state to protect. |
| Resource `FileWorker`, `FileSystem`, `FileInfo` | Adapt concept | Worker queues and resume metadata are coupled to chat IDs, base64 and client paths. Preserve the acknowledged-offset concept in NDT `ArchiveLogic` and `FileManager`, with validated IDs and binary gRPC chunks. |
| Per-service `message.proto` and generated `.pb.*` | Replace | Chat schemas are duplicated across services. The NDT runtime keeps one `proto/ndt.proto`, built by CMake into `generated/` and shared by all six processes. |

At baseline, `NdtAcqServer.cpp` has 450 lines, `NdtFileServer.cpp` 178,
`NdtStatusServer.cpp` 86, and the other services also combine transport,
state and domain operations. Their behavior and tests form the migration
baseline. In particular, `tests/full_chain_test.py` and
`tests/upload_recovery_test.py` protect the acquisition and archive path.
