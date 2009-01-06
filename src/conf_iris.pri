CONFIG -= debug_and_release
CONFIG -= debug
CONFIG -= release

unix:include(../conf.pri)
windows:include(../conf_windows.pri)

# don't build iris apps
CONFIG += no_tests

# use qca from psi if necessary
qca-static {
	DEFINES += QCA_STATIC
	INCLUDEPATH += $$PWD/../third-party/qca/qca/include/QtCrypto
}
else {
	CONFIG += crypto
}

# use zlib from psi if necessary
psi-zip {
	INCLUDEPATH += $$PWD/../src/tools/zip/minizip/win32
}

mac {
	# Universal binaries
	qc_universal:contains(QT_CONFIG,x86):contains(QT_CONFIG,ppc) {
		CONFIG += x86 ppc
		QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk

		# comment this out, iris already specifies 10.3, and i don't
		# think this has anything to do with universal, does it?
		#QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.4
	}
}
