#!/bin/sh

for f in `find botan -name \*.cpp -o -name \*.h` ; do
	echo "/*" > file.tmp
	cat botan/license.txt >> file.tmp
	echo "*/" >> file.tmp
	echo "// LICENSEHEADER_END" >> file.tmp
	cat $f >> file.tmp
	cp file.tmp $f
	rm file.tmp
done

