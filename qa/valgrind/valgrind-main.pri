unix {
	# valgrind target (only shows valgrind output)
	QMAKE_EXTRA_TARGETS += valgrind
	valgrind.depends = sub-src
	valgrind.commands = $(MAKE) -f src//$(MAKEFILE) valgrind

	# valgrind_supp target (generate suppressions)
	QMAKE_EXTRA_TARGETS += valgrind_supp
	valgrind_supp.depends = sub-src
	valgrind_supp.commands = $(MAKE) -f src//$(MAKEFILE) valgrind_supp

	# callgrind profiling
	QMAKE_EXTRA_TARGETS += callgrind
	callgrind.depends = sub-src
	callgrind.commands = $(MAKE) -f src//$(MAKEFILE) callgrind
}
