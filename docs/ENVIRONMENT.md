# Build environment audit (2026-10-10)

The source checkout is `D:\llfcproj\mychat`. The initial NDT implementation
was developed from `origin/master` on `codex/ndt-distributed-system`; this
architecture refactor is on `codex/architecture-refactor` from `laser/main`.

## Qt Creator and Kits

Qt Creator 4.11.0 is installed at `D:\qt\Tools\QtCreator`. Its saved Kit
configuration (`%APPDATA%\QtProject\qtcreator\profiles.xml`) lists Qt 5.9.9
MSVC2015 32-bit as the first/default desktop Kit and also lists MSVC2015
64-bit and MSVC2017 64-bit Kits. The corresponding qmake programs exist;
`D:\qt\5.9.9\msvc2015_64\bin\qmake.exe -v` reports Qt 5.9.9.
Qt Charts headers and DLLs exist for that 64-bit installation.

The saved desktop MSVC Kit entries refer to old compiler ABI identifiers,
including `x86-windows-msvc2015-pe-64bit`. VS2022 Community provides the
14.29, 14.40 and 14.44 MSVC toolsets, including an x64 `cl.exe` in 14.29;
the original 2015/2017 Visual Studio installations were not found. The saved
debugger list contains only MinGW GDB entries. The expected Windows SDK
`cdb.exe` was not found. Consequently a yellow Kit warning may reflect
compiler or debugger mismatch; the saved XML alone cannot prove the exact
live warning text or Kit readiness. Qt Creator's live Options > Kits page
was reopened during the refactor. It listed the default Desktop Qt 5.9.9
MSVC2015 32bit Kit and separate MSVC2015/2017 64bit Kits. The automation
could read the list but could not select an individual Kit or capture its
detail pane, so the exact yellow warning text remains unconfirmed. The
compiler/debugger findings above are independent toolchain checks, not an
assertion that an IDE Kit is valid.

Use the MSVC2015 64-bit Qt binaries with the installed VS2022 x64 toolchain
for command-line build verification. In Qt Creator, point the Kit's C and C++
compiler to an installed x64 MSVC toolchain, verify its Qt version and qmake,
and configure a matching CDB debugger if debugging is required. A successful
qmake/build is the final compiler compatibility check.

## Original server project paths

The original `.props` files (preserved in `origin/master`) refer to `D:\cppsoft\...` for Boost, gRPC,
jsoncpp, MySQL Connector and hiredis. Those paths do not exist here.
Some source files also embed Redis passwords and fixed addresses. They must
not be used unchanged. Local gRPC debug libraries under `D:\boost` were linked
successfully with VS2022 14.44, using the new CMake configuration. The
original Visual Studio solutions cannot be treated as reproducible builds.
