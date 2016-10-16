#CONFIG += release
QT += webkit
QMAKE_CXXFLAGS += -std=c++11

isEmpty(PSISDK) {
    include(../../psiplugin.pri)
} else {
    include($$PSISDK/plugins/psiplugin.pri)
}

RESOURCES = imagepreviewplugin.qrc

SOURCES += imagepreviewplugin.cpp \

