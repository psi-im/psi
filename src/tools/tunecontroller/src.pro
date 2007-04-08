TEMPLATE = lib
TARGET = tunecontroller
DESTDIR = ../lib
VERSION = 1
QT -= gui
CONFIG += dll 

MOC_DIR = .moc
OBJECTS_DIR = .obj

unix {
	include(../conf.pri)
}

mac {
	CONFIG += tc_itunes
}

TUNECONTROLLER_PATH = .
include($$TUNECONTROLLER_PATH/tunecontroller.pri)
