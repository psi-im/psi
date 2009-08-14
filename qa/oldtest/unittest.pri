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

unix {
	# gdb target
	QMAKE_EXTRA_TARGETS += gdb
	gdb.depends = $$EXEC_TARGET
	mac {
		QT_FRAMEWORK_VERSION = 4
		QT_FRAMEWORKS = QtCore QtXml QtNetwork QtGui QtSql
		FRAMEWORK = \$(QTDIR)/lib/\$\${f}.framework/Versions/$$QT_FRAMEWORK_VERSION/\$\${f}
		gdb.commands += \
			for f in $$QT_FRAMEWORKS; do \
				install_name_tool -id "$$FRAMEWORK" "$$FRAMEWORK""_debug"; \
				install_name_tool -change "$$FRAMEWORK" "$$FRAMEWORK""_debug" "./$$EXEC_TARGET"; \
			done;
	}
	gdb.commands += PSIDATADIR=~/.psi-test gdb ./$$EXEC_TARGET
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
