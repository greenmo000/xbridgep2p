
###############################################################

win32-msvc2013 {

message(win32-msvc2013 build)

CONFIG += breakpad

INCLUDEPATH += \
    d:/work/openssl/openssl/include \
    D:/work/boost/boost_1_57_0

LIBS += \
    -Ld:/work/openssl/openssl/lib \
    -LD:/work/boost/boost_1_57_0/stage/lib

LIBS += \
    -llibeay32 \
    -lssleay32
}

###############################################################
win32-g++ {

message(win32-g++ build)

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += \
    d:/work/openssl/openssl-1.0.2a-mgw/include \
    D:/work/boost/boost_1_57_0

LIBS += \
    -Ld:/work/openssl/openssl-1.0.2a-mgw \
    -LD:/work/boost/boost_1_57_0/stage/lib

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

###############################################################
linux-g++ {

message(linux-g++ build)

QMAKE_CXXFLAGS += -std=c++11

LIBS += \
    -lcrypto \
    -lssl \
    -lboost_system \
    -lboost_thread \
    -lboost_date_time \
    -lboost_program_options
}
