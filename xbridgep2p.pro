#-------------------------------------------------
#
# Project created by QtCreator 2014-11-20T11:31:03
#
#-------------------------------------------------

#QT       += core concurrent
#greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += console
CONFIG   -= app_bundle

TARGET = xbridgep2p
TEMPLATE = app

!include($$PWD/config.pri) {
    error(Failed to include config.pri)
}

DEFINES += \
    _CRT_SECURE_NO_WARNINGS \
    _SCL_SECURE_NO_WARNINGS

SOURCES += \
    src/main.cpp\
    src/dht/dht.cpp \
    src/util/util.cpp \
    src/util/logger.cpp \
    src/xbridgeapp.cpp \
    src/xbridge.cpp \
    src/xbridgesession.cpp \
    src/xbridgeexchange.cpp \
    src/xbridgetransaction.cpp \
    src/util/settings.cpp \
    src/xbridgetransactionmember.cpp

HEADERS += \
    src/dht/dht.h \
    src/util/util.h \
    src/util/logger.h \
    src/util/uint256.h \
    src/xbridgeapp.h \
    src/xbridge.h \
    src/xbridgesession.h \
    src/xbridgepacket.h \
    src/xbridgeexchange.h \
    src/xbridgetransaction.h \
    src/util/settings.h \
    src/xbridgetransactionmember.h \
    src/version.h \
    src/config.h

#LIBS += \
#    -llibeay32 \
#    -lssleay32


win32-g++ {

LIBS += \
    -lwsock32 \
    -lws2_32 \
    -lcrypto \
    -lssl \
    -lgdi32
}
