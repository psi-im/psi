include(../../plugins.pri)

SNARL_PATH = D:/devel/snarl/1.1/sdk/include/C++-kev


INCLUDEPATH += $$PSI_PATH \
		$$SNARL_PATH

SOURCES += snarlplugin.cpp \
		SnarlInterface.cpp

HEADERS += SnarlInterface.h
