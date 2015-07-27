
windows {

win32-msvc2013 {
INCLUDEPATH += \
    d:/work/openssl/openssl/include \
    D:/work/boost/boost_1_57_0

LIBS += \
    -Ld:/work/openssl/openssl/lib \
    -LD:/work/boost/boost_1_57_0/stage/lib
}

win32-g++ {

INCLUDEPATH += \
    d:/work/openssl/openssl-1.0.2a-mgw/include \
    D:/work/boost/boost_1_57_0

LIBS += \
    -Ld:/work/openssl/openssl-1.0.2a-mgw \
    -LD:/work/boost/boost_1_57_0/stage/lib
}
}

