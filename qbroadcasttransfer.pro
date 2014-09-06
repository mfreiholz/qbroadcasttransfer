QT += core gui widgets network
CONFIG -= flat

SOURCES += \
    src/main.cpp \
    src/server.cpp \
    src/protocol.cpp \
    src/client.cpp \
    src/gui/servercontrollerwidget.cpp \
    src/gui/clientcontrollerwidget.cpp

HEADERS += \
    src/server.h \
    src/api.h \
    src/protocol.h \
    src/client.h \
    src/gui/servercontrollerwidget.h \
    src/gui/clientcontrollerwidget.h

win32 {
    LIBS += Ws2_32.lib
}

RESOURCES += \
    res.qrc

FORMS += \
    src/gui/servercontrollerwidget.ui \
    src/gui/clientcontrollerwidget.ui
