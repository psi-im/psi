SOURCES = idleui.cpp
DLL_NAME = idleui.dll
DEF_NAME = idleui.def

all: $(DLL_NAME)

$(DLL_NAME): $(SOURCES)
 cl /nologo /MD $(SOURCES) /link /def:$(DEF_NAME) user32.lib kernel32.lib /dll /out:$(DLL_NAME)

