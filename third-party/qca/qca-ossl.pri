SOURCES += $$PWD/qca-ossl/qca-ossl.cpp

windows {
	LIBS += -lgdi32 -lwsock32
}
