#CONFIG += release
QT += webkit network
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += webkitwidgets
} 

isEmpty(PSISDK) {
    include(../../psiplugin.pri)
} else {
    include($$PSISDK/plugins/psiplugin.pri)
}

RESOURCES = imagepreviewplugin.qrc

SOURCES += imagepreviewplugin.cpp \

