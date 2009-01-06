CONFIG += console
CONFIG -= app_bundle
QT -= gui
QT += network

HEADERS += qdnssd.h
SOURCES += qdnssd.cpp sdtest.cpp

!mac:LIBS += -ldns_sd
