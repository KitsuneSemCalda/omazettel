QT += core gui concurrent quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omazettel

INCLUDEPATH += ../src
SOURCES += \
    tst_omazettel.cpp \
    ../src/backend.cpp \
    ../src/backend_notes.cpp \
    ../src/cli.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/markdownhighlighter_notes.cpp \
    ../src/exporter.cpp \
    ../src/noteindex.cpp \
    ../src/notetemplates.cpp \
    ../src/zettel.cpp \
    ../src/vault.cpp
HEADERS += \
    ../src/backend.h \
    ../src/cli.h \
    ../src/markdownhighlighter.h \
    ../src/exporter.h \
    ../src/noteindex.h \
    ../src/notetemplates.h \
    ../src/zettel.h \
    ../src/vault.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
