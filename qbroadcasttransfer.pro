QT += core gui network

SOURCES += \
    src/main.cpp \
    src/server.cpp \
    src/protocol.cpp \
    src/client.cpp \
    src/serverlogic.cpp

HEADERS += \
    src/server.h \
    src/api.h \
    src/protocol.h \
    src/client.h \
    src/serverlogic.h

win32 {
    LIBS += Ws2_32.lib
}

RESOURCES += \
    res.qrc
