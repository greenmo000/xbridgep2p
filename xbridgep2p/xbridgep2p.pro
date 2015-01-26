#-------------------------------------------------
#
# Project created by QtCreator 2014-11-20T11:31:03
#
#-------------------------------------------------

QT       += core gui concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = xbridgep2p
TEMPLATE = app

!include($$PWD/config.pri) {
    error(Failed to include config.pri)
}

QMAKE_CXXFLAGS += -std=c++11

DEFINES += \
    _CRT_SECURE_NO_WARNINGS \
    _SCL_SECURE_NO_WARNINGS

SOURCES += \
    src/main.cpp\
    src/statdialog.cpp \
    src/dht/dht.cpp \
    src/util.cpp \
    src/logger.cpp \
    src/xbridgeapp.cpp \
    src/xbridge.cpp \
    src/xbridgesession.cpp

HEADERS += \
    src/statdialog.h \
    src/dht/dht.h \
    src/util.h \
    src/logger.h \
    src/uint256.h \
    src/xbridgeapp.h \
    src/xbridge.h \
    src/xbridgesession.h \
    src/xbridgepacket.h

LIBS += \
    -llibeay32 \
    -lssleay32

win32-g++ {

LIBS += \
    -lboost_system-mgw48-mt-1_55 \
    -lboost_thread-mgw48-mt-1_55 \
    -lwsock32 \
    -lws2_32
}
