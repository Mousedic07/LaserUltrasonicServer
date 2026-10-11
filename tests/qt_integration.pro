QT += core gui widgets network charts
CONFIG += c++11 console
TEMPLATE = app
TARGET = qt_integration
INCLUDEPATH += ../client/ndt_client
SOURCES += qt_integration_main.cpp \
    ../client/ndt_client/NdtWindow.cpp \
    ../client/ndt_client/HttpMgr.cpp \
    ../client/ndt_client/TcpMgr.cpp
HEADERS += ../client/ndt_client/NdtWindow.h \
    ../client/ndt_client/HttpMgr.h \
    ../client/ndt_client/TcpMgr.h
DESTDIR = $$OUT_PWD/bin
