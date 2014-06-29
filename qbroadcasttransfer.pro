QT += qml quick network

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += src/main.cpp \
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

OTHER_FILES += \
		qml/server.qml

LIBS += Ws2_32.lib
