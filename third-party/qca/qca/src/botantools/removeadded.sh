#!/bin/sh

for f in `find botan -name \*.cpp -o -name \*.h` ; do
	sed -e '1,/LICENSEHEADER_END/d' $f | grep -v WRAPNS_LINE > file.tmp
	cp file.tmp $f
	rm file.tmp
done

