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

# run target (TODO: Make it work on Windows too)
QMAKE_EXTRA_TARGETS += run
run.depends = $$EXEC_TARGET
run.commands = PSIDATADIR=~/.psi-test ./$$EXEC_TARGET

# run_qa target
QA_DIR = $$PWD/../integration/psi-data/
QMAKE_EXTRA_TARGETS += run_qa
run_qa.depends = $$EXEC_TARGET
run_qa.commands = PSIDATADIR=$$QA_DIR ./$$EXEC_TARGET

unix {
	# gdb target
	QMAKE_EXTRA_TARGETS += gdb
	gdb.depends = $$EXEC_TARGET
	mac {
		gdb.commands += $$PWD/mac_qt_debug.rb true $$EXEC_TARGET;
	}
	gdb.commands += PSIDATADIR=~/.psi-test gdb ./$$EXEC_TARGET;
	mac {
		gdb.commands += $$PWD/mac_qt_debug.rb false $$EXEC_TARGET;
	}

	# gdb_qa target
	QMAKE_EXTRA_TARGETS += gdb_qa
	gdb_qa.depends = $$EXEC_TARGET
	mac {
		gdb_qa.commands += $$PWD/mac_qt_debug.rb true $$EXEC_TARGET;
	}
	gdb_qa.commands += PSIDATADIR=$$QA_DIR gdb ./$$EXEC_TARGET;
	mac {
		gdb_qa.commands += $$PWD/mac_qt_debug.rb false $$EXEC_TARGET;
	}
}

mac {
	# xcode target
	QMAKE_EXTRA_TARGETS += xcode
	xcode.depends = Makefile
	xcode.commands = ${QMAKE} -spec macx-xcode -o $$TARGET
}
