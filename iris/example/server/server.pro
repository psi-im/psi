TEMPLATE = app
CONFIG += qt thread console
TARGET = server

MOC_DIR        = .moc
OBJECTS_DIR    = .obj
UI_DIR         = .ui

include(../xmpptest/iris.pri)

SOURCES += server.cpp

# gentoo hack?
LIBS += -lcrypto

