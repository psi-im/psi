libidn {
	INCLUDEPATH += $$PWD
	DEPENDPATH  += $$PWD

	unix:{
		QMAKE_CFLAGS_WARN_ON -= -W
	}
	win32:{
		QMAKE_CFLAGS += -Zm400
	}

	SOURCES += \
		$$LIBIDN_BASE/profiles.c \
		#$$LIBIDN_BASE/toutf8.c \
		$$LIBIDN_BASE/rfc3454.c \
		$$LIBIDN_BASE/nfkc.c \
		$$LIBIDN_BASE/stringprep.c
}

