QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omazettel
TEMPLATE = app

HEADERS += \
    src/apptheme.h \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/citations.h \
    src/exporter.h \
    src/noteindex.h \
    src/notetemplates.h \
    src/sync.h \
    src/textdiff.h \
    src/systemtheme.h \
    src/vault.h \
    src/zettel.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/apptheme.cpp \
    src/backend_notes.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/markdownhighlighter_notes.cpp \
    src/citations.cpp \
    src/exporter.cpp \
    src/noteindex.cpp \
    src/notetemplates.cpp \
    src/sync.cpp \
    src/textdiff.cpp \
    src/systemtheme.cpp \
    src/vault.cpp \
    src/zettel.cpp

RESOURCES += src/resources.qrc
