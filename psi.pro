TEMPLATE = subdirs

include(conf.pri)
windows:include(conf_windows.pri)

# configure iris
unix:system("echo \"include(../src/conf_iris.pri)\" > iris/conf.pri")
windows:system("echo include(../src/conf_iris.pri) > iris\\conf.pri")

qca-static {
	sub_qca.subdir = third-party/qca
	sub_iris.depends = sub_qca

	SUBDIRS += sub_qca
}

sub_iris.subdir = iris
sub_src.subdir = src
sub_src.depends = sub_iris

SUBDIRS += \
	sub_iris \
	sub_src
