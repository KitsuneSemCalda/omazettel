QT += core gui concurrent quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_notes

INCLUDEPATH += ../../src
SOURCES += \
    tst_notes.cpp \
    ../../src/backend.cpp \
    ../../src/backend_notes.cpp \
    ../../src/markdownhighlighter.cpp \
    ../../src/markdownhighlighter_notes.cpp \
    ../../src/noteindex.cpp \
    ../../src/notetemplates.cpp \
    ../../src/zettel.cpp \
    ../../src/vault.cpp
HEADERS += \
    ../../src/backend.h \
    ../../src/markdownhighlighter.h \
    ../../src/noteindex.h \
    ../../src/notetemplates.h \
    ../../src/zettel.h \
    ../../src/vault.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
