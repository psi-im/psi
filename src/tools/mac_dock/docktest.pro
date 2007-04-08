TEMPLATE = app

HEADERS += mac_dock.h
SOURCES += mac_dock.cpp docktest.cpp

QMAKE_LFLAGS += -framework Carbon
