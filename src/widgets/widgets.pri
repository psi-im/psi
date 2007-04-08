INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

SOURCES += \
	$$PWD/busywidget.cpp \
	$$PWD/fancylabel.cpp \
	$$PWD/iconselect.cpp \
	$$PWD/iconwidget.cpp \ 
	$$PWD/psitextview.cpp \
	$$PWD/urllabel.cpp \
	$$PWD/urlobject.cpp \
	$$PWD/iconaction.cpp \
	$$PWD/fancypopup.cpp \
	$$PWD/psirichtext.cpp \
	$$PWD/psitooltip.cpp \
	$$PWD/psitabwidget.cpp

HEADERS += \
	$$PWD/busywidget.h \
	$$PWD/fancylabel.h \
	$$PWD/iconselect.h \
	$$PWD/iconsetselect.h \
	$$PWD/iconsetdisplay.h \
	$$PWD/iconwidget.h \
	$$PWD/iconbutton.h \
	$$PWD/psitextview.h \
	$$PWD/iconaction.h \
	$$PWD/fancypopup.h \
	$$PWD/urllabel.h \
	$$PWD/urlobject.h \
	$$PWD/iconlabel.h \
	$$PWD/icontoolbutton.h \
	$$PWD/fancypopuplist.h \
	$$PWD/psirichtext.h \
	$$PWD/psitooltip.h \
	$$PWD/psitabwidget.h 

# to remove dependency on iconset and stuff
#DEFINES += WIDGET_PLUGIN

# where to search for widgets plugin
#QMAKE_UIC += -L $$PWD
