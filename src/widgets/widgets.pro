TEMPLATE	= lib
CONFIG		+= designer plugin release
QT          += qt3support
TARGET		= psiwidgets

target.path	= $$[QT_INSTALL_PLUGINS]/designer
INSTALLS	+= target

# don't litter main dir
MOC_DIR     = .moc
OBJECTS_DIR = .obj
UI_DIR      = .ui

SOURCES += \
	psiwidgets.cpp \
	fancylabel.cpp \
	busywidget.cpp \
	iconwidget.cpp \
	iconaction.cpp \
	urlobject.cpp \
	urllabel.cpp \
	psirichtext.cpp \
	psitooltip.cpp \
	psitextview.cpp
#	psitabwidget.cpp \
#	psitabbar.cpp

HEADERS += \
	psiwidgets.h \
	fancylabel.h \
	busywidget.h \
	iconwidget.h \
	iconbutton.h \
	iconsetdisplay.h \
	iconsetselect.h \
	iconaction.h \
	urlobject.h \
	urllabel.h \
	iconlabel.h \
	icontoolbutton.h \
	psirichtext.h \
	psitooltip.h \
	psitextview.h
#	psitabwidget.h \
#	psitabbar.h

DISTFILES += \
	README \
	iconselect.cpp \
	iconselect.h

DEFINES += WIDGET_PLUGIN
