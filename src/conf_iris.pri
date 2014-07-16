include($$top_builddir/conf.pri)

# don't build iris apps
CONFIG += no_tests

mac {
	# Universal binaries
	qc_universal:contains(QT_CONFIG,x86):contains(QT_CONFIG,x86_64) {
		CONFIG += x86 x86_64
		QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.5.sdk
		QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
	}
}
