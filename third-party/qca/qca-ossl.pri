PSIDIR_TP = $$PSIDIR/third-party

SOURCES += $$PSIDIR_TP/qca/qca-ossl/qca-ossl.cpp

windows {
	LIBS += -lgdi32 -lwsock32 -llibeay32 -lssleay32
}
