PSIDIR_PRV = $$PWD/../../psi/src/privacy

INCLUDEPATH *= $$PWD $$PSIDIR_PRV
DEPENDPATH *= $$PWD $$PSIDIR_PRV

HEADERS += \
 	$$PSIDIR_PRV/privacylistitem.h \
 	$$PWD/privacylist_b.h \
 	$$PSIDIR_PRV/privacylistmodel.h \
 	$$PSIDIR_PRV/privacylistblockedmodel.h \
 	$$PSIDIR_PRV/privacymanager.h \
 	$$PWD/psiprivacymanager_b.h \
 	$$PSIDIR_PRV/privacydlg.h \
 	$$PSIDIR_PRV/privacyruledlg.h

SOURCES += \
 	$$PSIDIR_PRV/privacylistitem.cpp \
 	$$PWD/privacylist.cpp \
 	$$PSIDIR_PRV/privacylistmodel.cpp \
 	$$PSIDIR_PRV/privacylistblockedmodel.cpp \
 	$$PWD/psiprivacymanager.cpp \
 	$$PSIDIR_PRV/privacydlg.cpp \
 	$$PSIDIR_PRV/privacyruledlg.cpp

INTERFACES += \
	$$PSIDIR_PRV/privacy.ui \
	$$PSIDIR_PRV/privacyrule.ui
