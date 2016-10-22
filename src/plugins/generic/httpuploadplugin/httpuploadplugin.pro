#CONFIG += release
QT += network
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