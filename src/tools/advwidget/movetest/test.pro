TEMPLATE = app
CONFIG += qt advwidget
QT += gui
SOURCES += main.cpp
ADVWIDGET_CPP = ..
include($$ADVWIDGET_CPP/advwidget.pri)