QT *= network

include($$PSIMEDIA_DIR/psimedia/psimedia.pri)
INCLUDEPATH += $$PSIMEDIA_DIR/psimedia

DEFINES += GSTPROVIDER_STATIC
DEFINES *= QT_STATICPLUGIN
include($$PSIMEDIA_DIR/gstprovider/gstprovider.pri)

HEADERS += \
	$$PWD/jinglertp.h \
	$$PWD/calldlg.h

SOURCES += \
	$$PWD/jinglertp.cpp \
	$$PWD/calldlg.cpp

FORMS += \
	$$PWD/call.ui \
	$$PSIMEDIA_DIR/demo/config.ui
