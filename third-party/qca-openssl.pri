SOURCES += $$PWD/qca-openssl/qca-openssl.cpp

windows {
	LIBS += -lgdi32 -lwsock32 -llibeay32 -lssleay32
}
