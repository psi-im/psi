INCLUDEPATH *= $$PWD/..
DEPENDPATH *= $$PWD/.. $$PWD

FORMS += \
	$$PWD/CertificateDisplay.ui

HEADERS += \
	$$PWD/CertificateDisplayDialog.h \
	$$PWD/CertificateErrorDialog.h \
	$$PWD/CertificateHelpers.h

SOURCES += \
	$$PWD/CertificateDisplayDialog.cpp \
	$$PWD/CertificateErrorDialog.cpp \
	$$PWD/CertificateHelpers.cpp
