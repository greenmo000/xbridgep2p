#-------------------------------------------------
#
# Project created by QtCreator 2014-11-20T11:31:03
#
#-------------------------------------------------

#QT       += core concurrent
#greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += console
CONFIG   -= app_bundle
CONFIG   += static

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

win32-msvc2013 {
LIBS += \
    -llibeay32 \
    -lssleay32
}

win32-g++ {
QMAKE_CXXFLAGS += -std=c++11

LIBS += \
    -lwsock32 \
    -lws2_32 \
    -lcrypto \
    -lssl \
    -lgdi32 \
    -lboost_system-mgw49-mt-1_57 \
    -lboost_thread-mgw49-mt-1_57 \
    -lboost_date_time-mgw49-mt-1_57 \
    -lboost_program_options-mgw49-mt-1_57
}

gcc {
QMAKE_CXXFLAGS += -std=c++11

LIBS += \
    -lcrypto \
    -lssl \
    -lboost_system \
    -lboost_thread \
    -lboost_date_time \
    -lboost_program_options
}
