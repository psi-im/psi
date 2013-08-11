TEMPLATE = subdirs

include(conf.pri)

# configure iris
unix:system("echo \"include(../src/conf_iris.pri)\" > iris/conf.pri")
windows:system("echo include(../src/conf_iris.pri) > iris\\conf.pri")

sub_iris.subdir = iris
sub_src.subdir = src
sub_src.depends = sub_iris

SUBDIRS += \
	sub_iris \
	sub_src

OTHER_FILES += options/default.xml \
	options/macosx.xml \
	options/newprofile.xml \
	options/windows.xml

webkit {
	OTHER_FILES += themes/chatview/util.js \
		themes/chatview/psi/adapter.js \
		themes/chatview/adium/adapter.js
}
