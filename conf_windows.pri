# Windows build settings
CONFIG += debug
CONFIG += qca-static

# OpenSSL
qca-static {
	DEFINES += HAVE_OPENSSL
	DEFINES += OSSL_097
	OPENSSL_PREFIX = /local
	INCLUDEPATH += $$OPENSSL_PREFIX/include
	LIBS += -L$$OPENSSL_PREFIX/lib
}

# SASL
#qca-static {
#	CYRUSSASL_PREFIX = /local
#	INCLUDEPATH += $$CYRUSSASL_PREFIX/include
#	LIBS += $$CYRUSSASL_PREFIX/lib/libsasl.lib
#}
