INCLUDEPATH *= $$PWD
DEPENDPATH *= $$PWD

HEADERS += \
 	$$PWD/privacylistitem.h \
 	$$PWD/privacylist.h \
 	$$PWD/privacylistmodel.h \
 	$$PWD/privacylistblockedmodel.h \
 	$$PWD/privacymanager.h \
 	$$PWD/psiprivacymanager.h \
 	$$PWD/privacydlg.h \
 	$$PWD/privacyruledlg.h

SOURCES += \
 	$$PWD/privacylistitem.cpp \
 	$$PWD/privacylist.cpp \
 	$$PWD/privacylistmodel.cpp \
 	$$PWD/privacylistblockedmodel.cpp \
 	$$PWD/psiprivacymanager.cpp \
 	$$PWD/privacydlg.cpp \
 	$$PWD/privacyruledlg.cpp

FORMS += \
	$$PWD/privacy.ui \
	$$PWD/privacyrule.ui
