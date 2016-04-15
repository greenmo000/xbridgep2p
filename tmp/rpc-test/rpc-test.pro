#-------------------------------------------------
#
# Project created by QtCreator 2016-04-15T13:13:15
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = rpc-test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

#-------------------------------------------------
!include($$PWD/config.pri) {
    error(Failed to include config.pri)
}


SOURCES += main.cpp \
    bitcoinrpc.cpp \
    json/json_spirit_reader.cpp \
    json/json_spirit_value.cpp \
    json/json_spirit_writer.cpp \
    util/logger.cpp \
    util/util.cpp

HEADERS += \
    bitcoinrpc.h \
    json/json_spirit.h \
    json/json_spirit_error_position.h \
    json/json_spirit_reader.h \
    json/json_spirit_reader_template.h \
    json/json_spirit_stream_reader.h \
    json/json_spirit_utils.h \
    json/json_spirit_value.h \
    json/json_spirit_writer.h \
    json/json_spirit_writer_template.h \
    util/logger.h \
    util/util.h \
    util/verify.h

DISTFILES += \
    json/LICENSE.txt
