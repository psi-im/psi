# WARNING! All changes made in this file will be lost!
TEMPLATE = subdirs

SUBDIRS += \
	../src/tools/iconset/unittest \
	../src/widgets/unittest/iconaction \
	../src/widgets/unittest/richtext \
	../src/unittest/psiiconset \
	../src/unittest/psipopup

QMAKE_EXTRA_TARGETS += check
check.commands = sh ./checkall
