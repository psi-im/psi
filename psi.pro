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

unix {
	# unittest
	QMAKE_EXTRA_TARGETS += check
	check.commands += cd unittest && make check && cd ..
}

