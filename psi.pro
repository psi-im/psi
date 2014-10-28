TEMPLATE = subdirs

include(conf.pri)

# configure iris
unix:system("echo \"include($$top_srcdir/src/conf_iris.pri)\" > $$top_builddir/iris/conf.pri")
windows:system("echo include($$top_srcdir/src/conf_iris.pri) > $$top_builddir\\iris\\conf.pri")

sub_iris.subdir = iris
sub_src.subdir = src

!greaterThan(QT_MAJOR_VERSION, 4) {
	sub_initvars.file = initvars.pro
	sub_iris.depends = sub_initvars
	sub_src.depends = sub_initvars
	SUBDIRS += sub_initvars
}

sub_src.depends += sub_iris

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

# Import useful Makefile targets for testing program using valgrind.
# Extra targets: valgrind, valgrind_supp and callgrind are available
# only in unix systems.
include(qa/valgrind/valgrind-main.pri)

# Import useful Makefile targets for testing program with default config, for
# debugging program, etc. Extra targets: run, run_qa, gdb, gdb_qa and xcode.
include(qa/oldtest/unittest-main.pri)
