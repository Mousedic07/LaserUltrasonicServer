@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0.."
if not exist "build-client-integration" mkdir "build-client-integration"
cd /d "build-client-integration"
if not defined NDT_QT_ROOT set "NDT_QT_ROOT=D:\qt\5.9.9\msvc2015_64"
"%NDT_QT_ROOT%\bin\qmake.exe" "..\tests\qt_integration.pro" CONFIG+=release
if errorlevel 1 exit /b %errorlevel%
nmake
exit /b %errorlevel%
