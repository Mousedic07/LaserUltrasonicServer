# Build, run and verify on Windows

## Installed toolchains checked here

Qt Creator 4.11.0, Qt 5.9.9, MSVC2015 64-bit Qt libraries, Qt Charts and
VS2022 are installed. The VS2022 14.29 x64 toolset successfully built the
qmake client. The service binaries use VS2022 14.44 x64 because the existing
local gRPC debug libraries need a newer MSVC runtime. The installed 14.40
toolset is incomplete (`MSVCRTD.lib` missing). The saved Qt Creator Kits still
refer to older compiler IDs and no CDB debugger was found. Inspect the
yellow Kit warnings in **Tools > Options > Kits** and set C/C++ compiler,
Qt version and debugger explicitly. The live warning text could not be read
after Qt Creator UI inspection was interrupted; command-line builds do not
validate the IDE's debugger settings.

## Build

Run from a terminal with this repository as the working directory:

```bat
scripts\build_client_msvc64.bat
scripts\build_server_msvc64.bat
```

Open `client/ndt_client/ndt_client.pro` in Qt Creator 4.11.0 for editing,
building and GUI debugging. Choose the Desktop Qt 5.9.9 MSVC2015 64-bit Qt
version paired with a working x64 MSVC compiler. The Qt Charts module is
installed here. The executable is under `build-client/bin`. The server batch
uses the CMake file at repository root and produces six executables under
`build-server-v144`. Build intermediates remain in the project tree and are
Git-ignored.

The server CMake cache paths point at the locally installed Boost, gRPC and
MySQL Connector trees under `D:\boost`. Override `NDT_GRPC_SOURCE`,
`NDT_GRPC_BUILD`, `NDT_BOOST_ROOT`, and `NDT_MYSQL_ROOT` when moving to another
machine. The gRPC build directory must contain matching Debug static libs,
`protoc.exe`, `grpc_cpp_plugin.exe` and `zlibd.dll`.

## Local runtime configuration

Use environment variables in the launching terminal; no production database
secret is checked into the project. The client reads `config.ini` beside its
executable, copied from `client/ndt_client/config.example.ini` by the build
script. Change `gate/login_url` if GateServer uses another address.

| Variable | Default / purpose |
| --- | --- |
| `NDT_GATE_HOST`, `NDT_GATE_PORT` | `127.0.0.1`, `8080` HTTP |
| `NDT_AUTH_HOST`, `NDT_AUTH_PORT` | `127.0.0.1`, `50051` gRPC |
| `NDT_STATUS_HOST`, `NDT_STATUS_PORT` | `127.0.0.1`, `50052` gRPC |
| `NDT_PROC_HOST`, `NDT_PROC_PORT` | `127.0.0.1`, `50053` gRPC |
| `NDT_FILE_HOST`, `NDT_FILE_PORT` | `127.0.0.1`, `50054` gRPC |
| `NDT_ACQ_NODE_ID`, `NDT_ACQ_HOST`, `NDT_ACQ_PORT` | unique node ID, `127.0.0.1`, `7001` |
| `NDT_DEVICE_HOST`, `NDT_DEVICE_BASE_PORT` | `127.0.0.1`, `9000`; stage +1, DAQ +2 |
| `NDT_MYSQL_HOST`, `NDT_MYSQL_PORT` | `127.0.0.1`, `3306` |
| `NDT_MYSQL_DATABASE`, `NDT_MYSQL_USER`, `NDT_MYSQL_PASSWORD` | `ndt`, `ndt`, environment supplied |
| `NDT_REDIS_HOST`, `NDT_REDIS_PORT`, `NDT_REDIS_PASSWORD` | `127.0.0.1`, `6379`, optional environment supplied |
| `NDT_FILE_ROOT`, `NDT_SPOOL_ROOT` | `data`, `spool` under service working directory |
| `NDT_HEARTBEAT_TIMEOUT_MS` | `6000` |

Initialize the schema with `sql/schema.sql`. `scripts/init_test_user.py`
creates or resets local test user `demo` with a **new random password**, prints
it once and stores only a salted PBKDF2 hash. Set `NDT_MYSQL_ADMIN_USER` and
`NDT_MYSQL_ADMIN_PASSWORD` for the initializer. Use the printed password in
the Qt login. The default CLI path can be overridden with `NDT_MYSQL_CLI`.

Start MySQL and Redis on loopback, then AuthServer, StatusServer, ProcServer,
FileServer, and two copies of `simulator/devices.py` with base ports 9000 and
9100. Start two AcqServer processes with different `NDT_ACQ_NODE_ID`,
`NDT_ACQ_PORT` and matching `NDT_DEVICE_BASE_PORT`, then GateServer and the Qt
client. AcqServer probes its devices every two seconds; allow one heartbeat
before logging in. Each server needs its own terminal/process. For example:

```powershell
$env:NDT_ACQ_NODE_ID='acq1'
$env:NDT_ACQ_PORT='7001'
$env:NDT_DEVICE_BASE_PORT='9000'
.\build-server-v144\AcqServer.exe
```

Use `acq2`, port `7002`, base `9100` in another terminal. The simulator is:

```powershell
python -B .\simulator\devices.py --base-port 9000
```

The client and server may use different compilers because their boundary is
HTTP/TCP. Qt's GUI thread only receives async network signals.

## Automated checks

```powershell
python -B tests\simulator_test.py
python -B tests\service_test.py
python -B tests\upload_recovery_test.py
python -B tests\auth_db_failure_test.py
python -B tests\rpc_failure_test.py
python -B tests\qt_smoke.py
.\build-server-v144\fft_test.exe
.\build-server-v144\protocol_test.exe
python -B tests\full_chain_test.py
```

`service_test.py` launches real StatusServer, ProcServer, FileServer and Redis
processes, then checks low-load routing, tie rotation, stale-node rejection,
token validation, FFT RPC, upload reconnect/resume and saved SHA-256. The
`upload_recovery_test.py` runner seeds a partially uploaded file, restarts
AcqServer, and checks that its disk spool resumes from FileServer's ACK and
the completed archive hash matches. The
`full_chain_test.py` runner securely initializes an isolated MySQL data
directory under `mysql-data`, generates random root and demo passwords,
starts all processes and tests HTTP login, Acq TCP framing, waveforms, FFT
and archive. MySQL initialization can take several minutes on this machine.
The runner terminates its child processes and keeps diagnostic logs under
`data`, without recording generated passwords. Never claim a full-chain pass
unless this runner exits with status 0.

## Current verification

The Qt client and six server executables built successfully. The simulator,
FFT, TCP fragmentation, Status/Proc/File RPC and Auth login tests passed.
AcqServer restart upload recovery also passed with a 95,000-byte confirmed
offset and a matching final SHA-256.
`full_chain_test.py` exited with code 0 on 2026-10-10 and printed successful
HTTP login, two live AcqServer instances routed by connection load,
8 waveform frames, 8 FFT results, Redis-backed upload and matching
spool/archive SHA-256. The separate status test also verified stale-node
rejection and tie rotation. The Qt executable passed an offscreen startup
smoke test using the installed Qt 5.9.9 runtime. Missing MySQL and ProcServer
were verified to produce bounded, explicit RPC errors. Device disconnect and
timeout injection passed simulator tests.
