!exists(conf.pri) {

# Windows build settings
CONFIG += release
#CONFIG += qca-static

# tell iris to use our internal libz
CONFIG += psi-zip

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

# ASpell
#DEFINES += HAVE_ASPELL
contains(DEFINES, HAVE_ASPELL) {
	ASPELL_PREFIX = ../../../aspell
	INCLUDEPATH += "$$ASPELL_PREFIX/include"
	LIBS += -L"$$ASPELL_PREFIX/lib"
	LIBS += -laspell-15
}

}
