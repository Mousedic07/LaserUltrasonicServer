# Architecture

```text
Qt Widgets --HTTP/JSON--> GateServer --gRPC--> AuthServer --MySQL--> users
                              |
                              +--gRPC--> StatusServer <--heartbeat-- AcqServer 1/2
Qt Widgets <--NDT1 TCP frames------------------------> AcqServer
                                                        |--TCP--> laser/stage/DAQ simulators
                                                        |--gRPC--> ProcServer (FFT)
                                                        +--gRPC--> FileServer --disk + Redis
```

Each named server is a separate executable and process. The default local
ports are Gate 8080, Auth 50051, Status 50052, Proc 50053, File 50054 and
Acq 7001. A second AcqServer uses another port, node ID and device base port.
The HTTP and TCP links permit the Qt 5.9.9 client and VS2022 services to use
different MSVC toolsets.

GateServer accepts bounded HTTP requests through Boost.Beast. It calls
AuthServer for MySQL verification and StatusServer for a healthy node and
short-lived opaque token. AuthServer stores PBKDF2-HMAC-SHA256 password hashes
in MySQL and makes a new DB connection per request so DB errors surface.

StatusServer stores latest Acq heartbeat, live connection/task counts and
device readiness. Nodes older than `NDT_HEARTBEAT_TIMEOUT_MS` are excluded.
The score is connections + 2 × running tasks; equal scores rotate. Tokens
expire after five minutes. The demo runs one StatusServer process, so this
token store is in memory and sessions are lost if it restarts.

AcqServer uses an adapted llfcchat Asio IO service pool with a 16-session
admission limit. One scan task controls three real TCP device sockets.
Bounded eight-item queues separate acquisition from FFT RPC and archive RPC;
full queues pause acquisition. The raw binary spool remains on AcqServer disk
until the upload is verified. ProcServer performs an iterative radix-2 FFT;
FileServer writes 64 KiB or smaller chunks, acknowledges durable offsets,
stores progress in Redis and verifies SHA-256 before atomic rename.

The network trust boundary is intentionally local for this demo. HTTP, TCP,
gRPC, MySQL and Redis bind to loopback by default. Deploying across machines
requires transport encryption, firewall rules and a shared session store.
