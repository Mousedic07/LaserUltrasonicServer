@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29
if errorlevel 1 exit /b %errorlevel%
if not exist "D:\llfcproj\mychat\build-tests" mkdir "D:\llfcproj\mychat\build-tests"
cd /d "D:\llfcproj\mychat\build-tests"
cl /nologo /EHsc /std:c++17 /O2 /Fe:fft_test.exe "D:\llfcproj\mychat\server\ProcServer\Fft.cpp" "D:\llfcproj\mychat\tests\fft_test.cpp"
if errorlevel 1 exit /b %errorlevel%
fft_test.exe
exit /b %errorlevel%
