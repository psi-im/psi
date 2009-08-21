# unittest helpers
TARGET = testiconset
CONFIG += unittest
include($$PWD/../../../../qa/oldtest/unittest.pri)

QT += gui xml
RESOURCES +=  ../../../../iconsets.qrc
SOURCES += testiconset.cpp

# iconset library
DEFINES += NO_ICONSET_SOUND
ICONSET_CPP = ..
CONFIG += iconset
include($$ICONSET_CPP/iconset.pri)

# zip (for .jisp-processing)
PSICS_CPP = ../..
CONFIG += zip
ZIP_CPP = $$PSICS_CPP/zip
INCLUDEPATH += $$ZIP_CPP
include($$ZIP_CPP/zip.pri)

