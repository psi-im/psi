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

!isEmpty(PSIMEDIA_DIR) {
	SUBDIRS += $$PSIMEDIA_DIR/gstprovider/gstelements/static
}

SUBDIRS += \
	iris \
	src
