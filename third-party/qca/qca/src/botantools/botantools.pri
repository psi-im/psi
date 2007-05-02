BOTAN_BASE = $$PWD/botan

DEFINES += \
	BOTAN_TYPES_QT \
	BOTAN_TOOLS_ONLY \
	BOTAN_FIX_GDB \
	BOTAN_MINIMAL_BIGINT

DEFINES += BOTAN_MP_WORD_BITS=32
DEFINES += BOTAN_KARAT_MUL_THRESHOLD=12
DEFINES += BOTAN_KARAT_SQR_THRESHOLD=12

DEFINES += BOTAN_EXT_MUTEX_QT
unix:DEFINES += BOTAN_EXT_ALLOC_MMAP

INCLUDEPATH += $$BOTAN_BASE
SOURCES += \
	$$BOTAN_BASE/util.cpp \
	$$BOTAN_BASE/charset.cpp \
	$$BOTAN_BASE/parsing.cpp \
	$$BOTAN_BASE/exceptn.cpp \
	$$BOTAN_BASE/mutex.cpp \
	$$BOTAN_BASE/mux_qt/mux_qt.cpp \
	$$BOTAN_BASE/defalloc.cpp \
	$$BOTAN_BASE/mem_pool.cpp \
	$$BOTAN_BASE/libstate.cpp \
	$$BOTAN_BASE/modules.cpp \
	$$BOTAN_BASE/mp_comba.cpp \
	$$BOTAN_BASE/mp_mul.cpp \
	$$BOTAN_BASE/mp_mulop.cpp \
	$$BOTAN_BASE/mp_shift.cpp \
	$$BOTAN_BASE/mp_asm.cpp \
	$$BOTAN_BASE/mp_misc.cpp \
	$$BOTAN_BASE/divide.cpp \
	$$BOTAN_BASE/bit_ops.cpp \
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

