# llfcchat source audit and reuse

Upstream: [secondtonone1/llfcchat](https://github.com/secondtonone1/llfcchat),
GPL-3.0. The upstream `LICENSE` remains in the repository.

| Component | Observed implementation | NDT use |
| --- | --- | --- |
| `server/GateServer/HttpConnection`, `CServer` | Boost.Beast async HTTP accept/read/write | Preserve the HTTP transport design; replace chat routes. |
| `server/GateServer/LogicSystem` | Route table and JSON POST handlers | Replace registration, reset, mail and chat handlers with login. |
| `server/StatusServer/StatusServiceImpl` | gRPC server and configured ChatServer map | Replace static chat map with live AcqServer heartbeat table. Its load-selection code is commented out. |
| `server/ChatServer/CServer`, `CSession`, `AsioIOServicePool`, `MsgNode` | Asio TCP sessions, exact-size header/body reads, bounded send queue, IO pool | Adapt protocol and acquisition session handling. |
| `client/llfcchat/httpmgr`, `tcpmgr` | Async `QNetworkAccessManager` and `QTcpSocket` with signal/slot delivery | Reuse networking patterns in a separate Qt 5.9.9 qmake NDT client. The chat framing and handlers are replaced. |
| `origin/Season_2:server/ResourceServer/FileWorker`, `FileSystem`, `FileInfo` | Chat resource transfer and resume-related code | Inspect algorithms for resume and file writes; adapt only reusable pieces because the messages and storage are chat-specific. |
| `origin/Season_2:server/ChatServer/DistLock` | Redis distributed lock | Audit owner-checked release and expiry before considering reuse. |

Known defects or gaps from actual source inspection:

- `StatusServiceImpl::getChatServer` returns the first configured node; load
  comparison is commented out and there is no heartbeat freshness filter.
- `StatusServiceImpl::Login` appears to invert its Redis `Get` check, rejecting
  a found token and then comparing an empty token on a miss.
- `ChatServer/CSession` uses a two-`short` chat header and a chat-sized payload;
  it has no protocol version, 32-bit length or waveform framing.
- Each original service has its own `message.proto` copy; there is no single
  shared NDT schema.
- Original server project properties contain obsolete absolute paths and
  hard-coded local/remote credentials. The checkout has no AuthServer,
  AcqServer, ProcServer or NDT FileServer.
- `Season_2` adds ResourceServer and client `filetcpmgr`, but its file transfer
  remains coupled to chat identifiers and cannot simply replace an NDT upload
  service.

This audit describes source that was actually present. Build and runtime
verification are tracked separately in the build guide.
