# usage: make VERSION=1.2.3

UNAME := $(shell uname)

include package_info

ifeq ($(UNAME), Darwin)
PLATFORM = mac
all: default-mac
deps: deps-mac
else ifeq ($(UNAME), MINGW32_NT-6.1)
PLATFORM = win
all: default-mingw
deps: deps-mingw
else
all: default-unknown
deps: deps-unknown
endif

default-mac: dist-mac

default-mingw: dist-mingw

clean:
	rm -rf build out dist

distclean: clean
	rm -rf packages
ifeq ($(PLATFORM), mac)
	rm -f Psi-$(VERSION).dmg
else
	rm -f psi-$(VERSION)-win64.zip psi-$(VERSION)-win32.zip
endif

dist-mac: apply-version Psi-$(VERSION).dmg

dist-mingw: apply-version psi-$(VERSION)-win64.zip psi-$(VERSION)-win32.zip

deps-mac: packages/$(qca_mac_file) packages/$(growl_file) packages/$(gstbundle_mac_file) packages/$(psimedia_mac_file)

deps-mingw: packages/$(zlib_win_file) packages/$(qca_win_file) packages/$(openssl_win_file) packages/$(aspell_win_file) packages/$(gstbundle_win_file) packages/$(psimedia_win_file)

packages/$(zlib_win_file):
	mkdir -p packages deps
	../fetch.sh $(zlib_win_url) packages/$(zlib_win_file)
	cd deps && unzip ../packages/$(zlib_win_file) && cd ..

packages/$(qca_win_file):
	mkdir -p packages deps
	../fetch.sh $(qca_win_url) packages/$(qca_win_file)
	cd deps && unzip ../packages/$(qca_win_file) && cd ..

packages/$(openssl_win_file):
	mkdir -p packages deps
	../fetch.sh $(openssl_win_url) packages/$(openssl_win_file)
	cd deps && unzip ../packages/$(openssl_win_file) && cd ..

packages/$(aspell_win_file):
	mkdir -p packages deps
	../fetch.sh $(aspell_win_url) packages/$(aspell_win_file)
	cd deps && unzip ../packages/$(aspell_win_file) && cd ..

packages/$(gstbundle_win_file):
	mkdir -p packages deps
	../fetch.sh $(gstbundle_win_url) packages/$(gstbundle_win_file)
	cd deps && unzip ../packages/$(gstbundle_win_file) && cd ..

packages/$(psimedia_win_file):
	mkdir -p packages deps
	../fetch.sh $(psimedia_win_url) packages/$(psimedia_win_file)
	cd deps && unzip ../packages/$(psimedia_win_file) && cd ..

packages/$(qca_mac_file):
	mkdir -p packages deps
	../fetch.sh $(qca_mac_url) packages/$(qca_mac_file)
	cd deps && tar jxvf ../packages/$(qca_mac_file) && cd ..

packages/$(growl_file):
	mkdir -p packages deps
	../fetch.sh $(growl_url) packages/$(growl_file)
	cd deps && unzip ../packages/$(growl_file) && cd ..

packages/$(gstbundle_mac_file):
	mkdir -p packages deps
	../fetch.sh $(gstbundle_mac_url) packages/$(gstbundle_mac_file)
	cd deps && tar jxvf ../packages/$(gstbundle_mac_file) && cd ..

packages/$(psimedia_mac_file):
	mkdir -p packages deps
	../fetch.sh $(psimedia_mac_url) packages/$(psimedia_mac_file)
	cd deps && tar jxvf ../packages/$(psimedia_mac_file) && cd ..

apply-version:
	test "$(VERSION)" != "" && echo "$(VERSION)" > ../../version

build/uni/psi/ok: packages/$(qca_mac_file) packages/$(growl_file) packages/$(gstbundle_mac_file) packages/$(psimedia_mac_file)
	mkdir -p build
	./build_package.sh psi "" ""
	touch build/uni/psi/ok

build/i386/psi/ok: packages/$(zlib_win_file) packages/$(qca_win_file) packages/$(openssl_win_file) packages/$(aspell_win_file) packages/$(gstbundle_win_file) packages/$(psimedia_win_file)
	mkdir -p build
	./build_package.sh psi i386 $(PWD)/out
	touch build/i386/psi/ok

build/x86_64/psi/ok: packages/$(zlib_win_file) packages/$(qca_win_file) packages/$(openssl_win_file) packages/$(aspell_win_file) packages/$(gstbundle_win_file) packages/$(psimedia_win_file)
	mkdir -p build
	./build_package.sh psi x86_64 $(PWD)/out
	touch build/x86_64/psi/ok

dist/psi-$(VERSION)-mac: build/uni/psi/ok
	./prep_dist.sh "" dist/psi-$(VERSION)-mac

dist/psi-$(VERSION)-win64: build/x86_64/psi/ok
	./prep_dist.sh $(PWD)/out/x86_64 dist/psi-$(VERSION)-win64

dist/psi-$(VERSION)-win32: build/i386/psi/ok
	./prep_dist.sh $(PWD)/out/i386 dist/psi-$(VERSION)-win32

Psi-$(VERSION).dmg: dist/psi-$(VERSION)-mac
	./pack_dmg.sh Psi-$(VERSION).dmg Psi dist/psi-$(VERSION)-mac

psi-$(VERSION)-win64.zip: dist/psi-$(VERSION)-win64
	rm -f psi-$(VERSION)-win64.zip
	cd dist && zip -r ../psi-$(VERSION)-win64.zip psi-$(VERSION)-win64

psi-$(VERSION)-win32.zip: dist/psi-$(VERSION)-win32
	rm -f psi-$(VERSION)-win32.zip
	cd dist && zip -r ../psi-$(VERSION)-win32.zip psi-$(VERSION)-win32
