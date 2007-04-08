# helpers to reduce the size of unittest projects
unittest {
	TEMPLATE = app

	CONFIG += qtestlib console debug qt
	CONFIG -= app_bundle
}

windows {
	# otherwise we would get 'unresolved external _WinMainCRTStartup'
	# when compiling with MSVC
	MOC_DIR     = _moc
	OBJECTS_DIR = _obj
	UI_DIR      = _ui
	RCC_DIR     = _rcc
}
!windows {
	MOC_DIR     = .moc
	OBJECTS_DIR = .obj
	UI_DIR      = .ui
	RCC_DIR     = .rcc
}

mac:app_bundle {
	EXEC_TARGET = $${TARGET}.app/Contents/MacOS/$${TARGET}
}
else {
	EXEC_TARGET = $$TARGET
}

# check target (TODO: Make it work on Windows too)
QMAKE_EXTRA_TARGETS += check
check.depends = $$EXEC_TARGET
check.commands = ./$$EXEC_TARGET

unix {
	# valgrind target (only shows valgrind output)
	VALGRIND_OPTIONS = -q --num-callers=40 --leak-check=full --show-reachable=yes --suppressions=$$PWD/valgrind.supp
	QMAKE_EXTRA_TARGETS += valgrind
	valgrind.depends = $$EXEC_TARGET
	valgrind.commands = valgrind $$VALGRIND_OPTIONS ./$$EXEC_TARGET | grep -E '==\d+=='

	# valgrind_supp target (generate suppressions)
	QMAKE_EXTRA_TARGETS += valgrind_supp
	valgrind_supp.depends = $$EXEC_TARGET
	valgrind_supp.commands = valgrind $$VALGRIND_OPTIONS --gen-suppressions=all ./$$EXEC_TARGET

	# gdb target
	QMAKE_EXTRA_TARGETS += gdb
	gdb.depends = $$EXEC_TARGET
	mac {
		QT_FRAMEWORK_VERSION = 4.0
		QT_FRAMEWORKS = QtCore QtXml QtNetwork QtGui QtSql Qt3Support
		FRAMEWORK = $(QTDIR)/lib/\$\$f.framework/Versions/$$QT_FRAMEWORK_VERSION/\$\$f
		gdb.commands += \
			for f in $$QT_FRAMEWORKS; do \
				install_name_tool -id "$$FRAMEWORK" "$$FRAMEWORK""_debug"; \
				install_name_tool -change "$$FRAMEWORK" "$$FRAMEWORK""_debug" "./$$EXEC_TARGET"; \
			done;
	}
	gdb.commands += gdb ./$$EXEC_TARGET
	mac {
		gdb.commands += ; \
			for f in $$QT_FRAMEWORKS; do \
				install_name_tool -id "$$FRAMEWORK""_debug" "$$FRAMEWORK""_debug"; \
				install_name_tool -change "$$FRAMEWORK""_debug" "$$FRAMEWORK" "./$$EXEC_TARGET"; \
			done;
	}
}

mac {
	# xcode target
	QMAKE_EXTRA_TARGETS += xcode
	xcode.depends = Makefile
	xcode.commands = ${QMAKE} -spec macx-xcode -o $$TARGET
}