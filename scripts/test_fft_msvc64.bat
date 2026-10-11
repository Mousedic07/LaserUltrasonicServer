@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0.."
if not exist "build-tests" mkdir "build-tests"
cd /d "build-tests"
cl /nologo /EHsc /std:c++17 /O2 /Fe:fft_test.exe "..\server\logic\ProcServer\Fft.cpp" "..\tests\fft_test.cpp"
if errorlevel 1 exit /b %errorlevel%
fft_test.exe
exit /b %errorlevel%
