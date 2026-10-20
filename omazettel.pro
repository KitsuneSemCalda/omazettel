QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omazettel
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/noteindex.h \
    src/systemtheme.h \
    src/vault.h \
    src/zettel.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/backend_notes.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/markdownhighlighter_notes.cpp \
    src/noteindex.cpp \
    src/systemtheme.cpp \
    src/vault.cpp \
    src/zettel.cpp

RESOURCES += src/resources.qrc
