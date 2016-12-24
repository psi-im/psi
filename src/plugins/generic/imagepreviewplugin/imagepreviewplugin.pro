#CONFIG += release
QT += webkit network
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += webkitwidgets
    CONFIG += c++11
} 

isEmpty(PSISDK) {
    include(../../psiplugin.pri)
} else {
    include($$PSISDK/plugins/psiplugin.pri)
}

RESOURCES = imagepreviewplugin.qrc

SOURCES += imagepreviewplugin.cpp \
           ScrollKeeper.cpp
