# llfcchat source audit and reuse

Upstream: [secondtonone1/llfcchat](https://github.com/secondtonone1/llfcchat),
GPL-3.0. The upstream `LICENSE` remains in the repository.
The paths in the table refer to `origin/master` or `origin/Season_2` during
the audit. After adapting the necessary transport code, unused chat UI,
chat servers, mail verification code, duplicate message schemas and old chat
SQL backups were removed from the NDT branch. They remain in `master` and
Git history for comparison and recovery. Original course documentation and
README remain as historical reference.

| Component | Observed implementation | NDT use |
| --- | --- | --- |
| `server/GateServer/HttpConnection`, `CServer` | Boost.Beast async HTTP accept/read/write | Preserve the HTTP transport design; replace chat routes. |
| `server/GateServer/LogicSystem` | Route table and JSON POST handlers | Replace registration, reset, mail and chat handlers with login. |
| `server/StatusServer/StatusServiceImpl` | gRPC server and configured ChatServer map | Replace static chat map with live AcqServer heartbeat table. Its load-selection code is commented out. |
| `server/ChatServer/CServer`, `CSession`, `AsioIOServicePool`, `MsgNode` | Asio TCP sessions, exact-size header/body reads, bounded send queue, IO pool | `server/common/AsioIOServicePool` is a direct adaptation of the original pool; AcqServer also adapts its exact-length framing and bounded queue approach. |
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

The new FileServer retains the Season_2 resume concept (server-confirmed
offset plus disk temp file), but it does not copy `FileWorker`: that class
handles chat resource metadata, its TCP messages and SQL tables. The NDT
archive instead uses the shared gRPC schema, Redis progress and a fixed raw
waveform record. The new AuthServer also uses a small dedicated DAO query
instead of the old chat registration DAO, because those methods store and
compare plaintext chat passwords.

This audit describes source that was actually present. Build and runtime
verification are tracked separately in the build guide.

## Checked call paths

- Original GateServer: `main` starts `CServer`; `HttpConnection::Start` reads
  with Beast; `HandleReq` invokes `LogicSystem::HandlePost`. The `/user_login`
  callback calls `MysqlMgr::CheckPwd` and then
  `StatusGrpcClient::GetChatServer`. New GateServer preserves the Beast HTTP
  transport and the Auth→Status ordering, replacing the chat DAO and routes.
- Original StatusServer: `StatusServiceImpl::GetChatServer` calls
  `getChatServer`, which immediately chooses the first configured map node;
  its Redis connection-count comparison is commented out. It writes a token
  through `insertToken`. The NDT StatusServer replaces that logic with live
  heartbeat filtering, scoring and token expiry.
- Original ChatServer: `CServer::HandleAccept` starts `CSession`, whose
  `AsyncReadHead`/`AsyncReadBody` perform exact-size reads and post to
  `LogicSystem::PostMsgToQue`. `AsioIOServicePool::GetIOService` rotates IO
  contexts. The adapted pool fixes its constructor race and stop behavior;
  the NDT frame reader adds magic, version and 32-bit length validation.
- Season_2 ResourceServer: `CSession` dispatches file messages to
  `FileWorker::PostTask`, whose `ID_UPLOAD_FILE_REQ` handler base64-decodes
  chat payloads and uses the supplied path. The worker queue has no explicit
  bound. Copying it would retain chat IDs, base64 overhead and path risk, so
  FileServer keeps the server-acknowledged resume idea with bounded binary
  gRPC chunks and validated server-derived paths.
- Season_2 `DistLock` already uses `SET ... NX EX` and Lua compare-and-delete,
  which correctly expires a lock and checks the owner on release. The demo
  uses one StatusServer instance; adding a lock without a shared multi-instance
  status store would not improve correctness, so no lock is in its runtime
  path. A multi-instance StatusServer would need shared heartbeat/session data
  before adopting that lock.
