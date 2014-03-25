DEPENDS = sub-src
COMMAND = $(MAKE) -f src//$(MAKEFILE)

# run target
QMAKE_EXTRA_TARGETS += run
run.depends = $$DEPENDS
run.commands = $$COMMAND run

# run_qa target
QMAKE_EXTRA_TARGETS += run_qa
run_qa.depends = $$DEPENDS
run_qa.commands = $$COMMAND run_qa

unix {
	# gdb target
	QMAKE_EXTRA_TARGETS += gdb
	gdb.depends = $$DEPENDS
	gdb.commands = $$COMMAND gdb

	# gdb_qa target
	QMAKE_EXTRA_TARGETS += gdb_qa
	gdb_qa.depends = $$DEPENDS
	gdb_qa.commands = $$COMMAND gdb_qa
}

mac {
	# xcode target
	QMAKE_EXTRA_TARGETS += xcode
	xcode.depends =  src//$(MAKEFILE)
	xcode.commands = $$COMMAND xcode
}
