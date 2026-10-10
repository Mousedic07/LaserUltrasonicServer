@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
if errorlevel 1 exit /b %errorlevel%
cd /d "D:\llfcproj\mychat"
cmake -S . -B build-server-v144 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b %errorlevel%
cmake --build build-server-v144 --parallel 4
exit /b %errorlevel%
