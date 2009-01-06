TEMPLATE = subdirs

include(conf.pri)
windows:include(conf_windows.pri)

# configure iris
unix:system("echo \"include(../src/conf_iris.pri)\" > iris/conf.pri")
windows:system("echo include(../src/conf_iris.pri) > iris\\conf.pri")

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
		qa/unittest \
		qa/guitest

	QMAKE_EXTRA_TARGETS += check
	check.commands += make -C qa/unittest check
}
