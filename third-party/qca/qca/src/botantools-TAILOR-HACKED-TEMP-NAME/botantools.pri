BOTAN_BASE = $$PWD/botan

DEFINES += \
	BOTAN_TYPES_QT \
	BOTAN_NO_INIT_H \
	BOTAN_NO_CONF_H \
	BOTAN_TOOLS_ONLY \
	BOTAN_MINIMAL_BIGINT

DEFINES += BOTAN_MP_WORD_BITS=32
DEFINES += BOTAN_VECTOR_OVER_ALLOCATE=4

INCLUDEPATH += $$BOTAN_BASE
SOURCES += \
	$$BOTAN_BASE/util.cpp \
	$$BOTAN_BASE/exceptn.cpp \
	$$BOTAN_BASE/mutex.cpp \
	$$BOTAN_BASE/mux_qt/mux_qt.cpp \
	$$BOTAN_BASE/secalloc.cpp \
	$$BOTAN_BASE/defalloc.cpp \
	$$BOTAN_BASE/allocate.cpp \
	$$BOTAN_BASE/mp_core.cpp \
	$$BOTAN_BASE/mp_comba.cpp \
	$$BOTAN_BASE/mp_fkmul.cpp \
	$$BOTAN_BASE/mp_mul.cpp \
	$$BOTAN_BASE/mp_smul.cpp \
	$$BOTAN_BASE/mp_shift.cpp \
	$$BOTAN_BASE/mp_misc.cpp \
	$$BOTAN_BASE/numthry.cpp \
	$$BOTAN_BASE/divide.cpp \
	$$BOTAN_BASE/big_base.cpp \
	$$BOTAN_BASE/big_code.cpp \
	$$BOTAN_BASE/big_io.cpp \
	$$BOTAN_BASE/big_ops2.cpp \
	$$BOTAN_BASE/big_ops3.cpp

unix:{
	SOURCES += \
		$$BOTAN_BASE/ml_unix/mlock.cpp \
		$$BOTAN_BASE/alloc_mmap/mmap_mem.cpp
}

windows:{
	SOURCES += \
		$$BOTAN_BASE/ml_win32/mlock.cpp
}

