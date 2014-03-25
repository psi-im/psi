DEPENDS = sub-src
COMMAND = $(MAKE) -f src//$(MAKEFILE)

unix {
	# valgrind target (only shows valgrind output)
	QMAKE_EXTRA_TARGETS += valgrind
	valgrind.depends = $$DEPENDS
	valgrind.commands = $$COMMAND valgrind

	# valgrind_supp target (generate suppressions)
	QMAKE_EXTRA_TARGETS += valgrind_supp
	valgrind_supp.depends = $$DEPENDS
	valgrind_supp.commands = $$COMMAND valgrind_supp

	# callgrind profiling
	QMAKE_EXTRA_TARGETS += callgrind
	callgrind.depends = $$DEPENDS
	callgrind.commands = $$COMMAND callgrind
}
