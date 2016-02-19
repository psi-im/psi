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
	$$PWD/psitiplabel.cpp \
	$$PWD/psitabwidget.cpp \
	$$PWD/psitabbar.cpp \
	$$PWD/actionlineedit.cpp \
	$$PWD/typeaheadfind.cpp


HEADERS += \
	$$PWD/stretchwidget.h \
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
	$$PWD/updatingcombobox.h \
	$$PWD/iconlabel.h \
	$$PWD/icontoolbutton.h \
	$$PWD/fancypopuplist.h \
	$$PWD/psirichtext.h \
	$$PWD/psitooltip.h \
	$$PWD/psitiplabel.h \
	$$PWD/psitabwidget.h \
	$$PWD/psitabbar.h \
	$$PWD/actionlineedit.h \
	$$PWD/typeaheadfind.h

FORMS += $$PWD/fancypopup.ui

# to remove dependency on iconset and stuff
#DEFINES += WIDGET_PLUGIN

# where to search for widgets plugin
#QMAKE_UIC += -L $$PWD
