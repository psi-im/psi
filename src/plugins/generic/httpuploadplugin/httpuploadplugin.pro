#CONFIG += release
CONFIG += c++11
QMAKE_CXXFLAGS += -std=c++11

isEmpty(PSISDK) {
    include(../../psiplugin.pri)
} else {
    include($$PSISDK/plugins/psiplugin.pri)
}

RESOURCES = httpuploadplugin.qrc

SOURCES += httpuploadplugin.cpp \
		   uploadservice.cpp \
		   previewfiledialog.cpp \

HEADERS += previewfiledialog.h