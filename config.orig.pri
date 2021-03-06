
###############################################################

win32-msvc2015 {

message(win32-msvc2015 build)

CONFIG += withbreakpad

INCLUDEPATH += \
    C:/_buildagent/src/3rdparty/openssl/openssl-1.0.2d/inc32 \
    c:/_buildagent/src/3rdparty/boost/boost_1_58_0

LIBS += \
    -LC:/_buildagent/src/3rdparty/openssl/openssl-1.0.2d/out32dll \
    -Lc:/_buildagent/src/3rdparty/boost/boost_1_58_0/stage/lib

LIBS += \
    -llibeay32 \
    -lssleay32
}

###############################################################

win32-msvc2013 {

contains(QMAKE_TARGET.arch, x86_64) {
    message(win32-msvc2013 build x64)
}
else {
    message(win32-msvc2013 build)
}

#CONFIG += withbreakpad
#CONFIG += withoutgui

INCLUDEPATH += \
    d:/work/openssl/openssl/include \
    D:/work/boost/boost_1_60_0

contains(QMAKE_TARGET.arch, x86_64) {
    QMAKE_CXXFLAGS += /bigobj
    QMAKE_CFLAGS += /bigobj

    LIBS += \
        -Ld:/work/openssl/openssl/lib64 \
        -LD:/work/boost/boost_1_60_0/stage64/lib
}
else {
    LIBS += \
        -Ld:/work/openssl/openssl/lib \
        -LD:/work/boost/boost_1_60_0/stage/lib

    LIBS += \
        -llibeay32 \
        -lssleay32

}

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

QMAKE_CXXFLAGS += -std=c++11 -pthread
QMAKE_LFLAGS += -pthread

LIBS += \
    -lcrypto \
    -lssl \
    -lboost_system \
    -lboost_thread \
    -lboost_date_time \
    -lboost_program_options \
    -lboost_filesystem
}

###############################################################
linux-g++-64 {

message(linux-g++-64 build)

CONFIG += withoutgui

QMAKE_CXXFLAGS += -std=c++11 -pthread
QMAKE_LFLAGS += -pthread

LIBS += \
    -lpthread \
    -lcrypto \
    -lssl \
    -lboost_system \
    -lboost_thread \
    -lboost_date_time \
    -lboost_program_options \
    -lboost_filesystem
}
