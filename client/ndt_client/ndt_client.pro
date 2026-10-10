QT += core gui widgets network charts
CONFIG += c++11
TEMPLATE = app
TARGET = ndt_client
DEFINES += QT_DEPRECATED_WARNINGS
SOURCES += main.cpp NdtWindow.cpp HttpMgr.cpp TcpMgr.cpp
HEADERS += NdtWindow.h HttpMgr.h TcpMgr.h
DESTDIR = $$OUT_PWD/bin
