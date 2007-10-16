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
	src

tests {
	SUBDIRS += \
		qa/unittest \
		qa/guitest

	QMAKE_EXTRA_TARGETS += check
	check.commands += make -C qa/unittest check
}
