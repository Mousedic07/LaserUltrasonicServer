# Build and run on Windows

## Toolchains checked on this machine

Qt Creator 4.11.0 and Qt 5.9.9 are installed. The default saved Kit is
MSVC2015 32-bit; the Qt 5.9.9 MSVC2015 64-bit qmake and Qt Charts module
also exist. VS2022 Community has x64 MSVC 14.29 and 14.44. The 14.29
toolset builds the x64 Qt client; 14.44 builds servers against this machine's
existing gRPC Debug libraries. Saved Qt Creator Kit compiler IDs are old and
no CDB debugger was found. Check the yellow Kit warnings in **Tools >
Options > Kits** before selecting a Kit. The exact live warning was not
confirmed during the earlier interrupted UI inspection; command-line
compilation alone does not validate Qt Creator debugging. See
[ENVIRONMENT.md](ENVIRONMENT.md).

## Build

From this repository root:

```bat
scripts\build_client_msvc64.bat
scripts\build_server_msvc64.bat
scripts\build_qt_integration_msvc64.bat
```

The Qt client remains a Qt Widgets C++ qmake `.pro` project at
`client/ndt_client/ndt_client.pro`, compatible with Qt 5.9.9 and Qt Creator
4.11.0. Qt Charts is used for waveform and FFT plots. The batch scripts use
their own location to find the checkout; set `NDT_QT_ROOT` to another x64 Qt
5.9.9 installation when needed. `build-client/bin/ndt_client.exe` is the
application and `build-client-integration/bin/qt_integration.exe` is an
offscreen test harness that compiles the same client sources.

The server CMake project builds six executables plus test tools under
`build-server-v144`. On this machine the cache defaults point to local Boost,
gRPC and MySQL Connector installations under `D:\boost`; override
`NDT_BOOST_ROOT`, `NDT_GRPC_SOURCE`, `NDT_GRPC_BUILD`, and `NDT_MYSQL_ROOT`
with `-D` options on other machines. The local gRPC build contains Debug
libraries, `protoc.exe`, `grpc_cpp_plugin.exe`, and `zlibd.dll`. Keep its
Protobuf/gRPC compiler and linked libraries from the same build/version.
For a clean generation/link check from an empty directory, open a VS2022
x64 developer terminal using MSVC 14.44 and run:

```bat
cmake -S . -B build-server-clean -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-server-clean --parallel 4
ctest --test-dir build-server-clean --output-on-failure
```

CMake generates `ndt.pb.h/.cc` and `ndt.grpc.pb.h/.cc` from the one
`proto/ndt.proto` into `<build-directory>/generated` and links them through
the `ndt_proto` target. Generated files are not committed.

## Configuration and launch

Each service calls `ConfigMgr::Load` at startup. Copy its
`config/<Service>/config.example.ini` to an ignored `config.ini` beside it,
or use `--config <path>` or `NDT_CONFIG`. The default search is relative to
the process working directory, so start services from the repository root.
`ConfigMgr` reads strings, integers, floating-point values and booleans,
applies documented defaults, and throws for required or malformed fields.
Sensitive values can be overridden by environment variables:

| Variable | Used by |
| --- | --- |
| `NDT_JWT_SECRET` | GateServer and AcqServer; same random value of at least 32 characters |
| `NDT_MYSQL_PASSWORD` | AuthServer |
| `NDT_REDIS_PASSWORD` | FileServer, when Redis authentication is enabled |

The test runners also override ports, device bases and storage roots through
the `NDT_*` variables in `ConfigMgr`. Do not commit local `config.ini` files
or a signing key. Gate and Acq must agree on issuer and audience; the
examples use `LaserUltrasonicServer` and `ndt-acq`.

Start local MySQL and Redis, initialize `sql/schema.sql`, then start
AuthServer, StatusServer, ProcServer, FileServer, device simulators,
AcqServer(s), GateServer and finally the Qt client. For example:

```powershell
$env:NDT_JWT_SECRET = '<your 32+ character random shared secret>'
$env:NDT_MYSQL_PASSWORD = '<local database password>'
$env:NDT_ACQ_NODE_ID = 'acq1'
$env:NDT_ACQ_PORT = '7001'
$env:NDT_DEVICE_BASE_PORT = '9000'
python -B .\simulator\devices.py --base-port 9000
# Run AcqServer in a separate terminal with the same environment:
.\build-server-v144\AcqServer.exe
```

A second AcqServer needs a distinct node ID, port and simulator base port.
The client reads `build-client/bin/config.ini` for the Gate login URL. The
client and servers may use different compilers because the boundary is
HTTP/TCP.

## Verification

Run non-conflicting integration scripts sequentially because they bind
fixed local ports:

```powershell
ctest --test-dir build-server-v144 --output-on-failure
python -B tests\simulator_test.py
python -B tests\service_test.py
python -B tests\async_io_test.py
python -B tests\redis_reconnect_test.py
python -B tests\upload_recovery_test.py
python -B tests\auth_db_failure_test.py
python -B tests\rpc_failure_test.py
python -B tests\qt_smoke.py
python -B tests\full_chain_test.py
```

`full_chain_test.py` starts isolated MySQL and Redis, both simulated device
sets, all six server processes and the Qt offscreen harness. It checks
MySQL pool reconnection after database restart, real login/route/JWT,
node-specific JWT rejection, TCP acquisition, FFT charts and archive hash.
It writes `data/full-chain.log` and does not log generated passwords.
`service_test.py` checks route scoring, tie rotation and stale heartbeats,
FFT RPC, binary upload offsets and SHA-256. `upload_recovery_test.py`
checks AcqServer restart from a saved upload offset. The test runners create
their own temporary ports and terminate their child processes.
