QT += core gui widgets network
CONFIG -= flat

SOURCES += \
    src/main.cpp \
    src/server.cpp \
    src/protocol.cpp \
    src/client.cpp \
    src/serverlogic.cpp \
    src/gui/servercontrollerwidget.cpp

HEADERS += \
    src/server.h \
    src/api.h \
    src/protocol.h \
    src/client.h \
    src/serverlogic.h \
    src/gui/servercontrollerwidget.h

win32 {
    LIBS += Ws2_32.lib
}

RESOURCES += \
    res.qrc

FORMS += \
    src/gui/servercontrollerwidget.ui
