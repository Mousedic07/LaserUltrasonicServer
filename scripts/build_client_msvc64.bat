@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29
if errorlevel 1 exit /b %errorlevel%
if not exist "D:\llfcproj\mychat\build-client" mkdir "D:\llfcproj\mychat\build-client"
cd /d "D:\llfcproj\mychat\build-client"
"D:\qt\5.9.9\msvc2015_64\bin\qmake.exe" "D:\llfcproj\mychat\client\ndt_client\ndt_client.pro" CONFIG+=release
if errorlevel 1 exit /b %errorlevel%
nmake
if errorlevel 1 exit /b %errorlevel%
copy /Y "D:\llfcproj\mychat\client\ndt_client\config.example.ini" "D:\llfcproj\mychat\build-client\bin\config.ini" >nul
exit /b 0
