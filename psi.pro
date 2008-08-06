TEMPLATE = subdirs

include(conf.pri)
windows:include(conf_windows.pri)

jingle {
	SUBDIRS += third-party/libjingle
}

qca-static {
	SUBDIRS += third-party/qca
}

SUBDIRS += \
	iris \
	src

tests {
	SUBDIRS += \
    third-party/cppunit \
		qa/unittest \
		qa/guitest

	QMAKE_EXTRA_TARGETS += check
	check.commands += make -C qa/unittest check
}
