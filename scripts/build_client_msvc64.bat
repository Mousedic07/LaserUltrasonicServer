@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0.."
if not exist "build-client" mkdir "build-client"
cd /d "build-client"
if not defined NDT_QT_ROOT set "NDT_QT_ROOT=D:\qt\5.9.9\msvc2015_64"
"%NDT_QT_ROOT%\bin\qmake.exe" "..\client\ndt_client\ndt_client.pro" CONFIG+=release
if errorlevel 1 exit /b %errorlevel%
nmake
if errorlevel 1 exit /b %errorlevel%
copy /Y "..\client\ndt_client\config.example.ini" "bin\config.ini" >nul
exit /b 0
