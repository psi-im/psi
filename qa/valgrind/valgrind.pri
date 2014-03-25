unix {
	# valgrind target (only shows valgrind output)
	VALGRIND_OPTIONS = -q --num-callers=40 --leak-check=full --show-reachable=yes --suppressions=$$PWD/valgrind.supp
	QMAKE_EXTRA_TARGETS += valgrind
	valgrind.depends = $$TARGET
	valgrind.commands = valgrind $$VALGRIND_OPTIONS $$DESTDIR/$$TARGET | grep -E '==[0-9]+=='

	# valgrind_supp target (generate suppressions)
	QMAKE_EXTRA_TARGETS += valgrind_supp
	valgrind_supp.depends = $$TARGET
	valgrind_supp.commands = valgrind $$VALGRIND_OPTIONS --gen-suppressions=all $$DESTDIR/$$TARGET

	# callgrind profiling
	QMAKE_EXTRA_TARGETS += callgrind
	callgrind.depends = $$TARGET
	callgrind.commands = valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes $$DESTDIR/$$TARGET
}
