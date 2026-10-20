QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omazettel
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/systemtheme.h \
    src/vault.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/backend_notes.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp \
    src/vault.cpp

RESOURCES += src/resources.qrc
