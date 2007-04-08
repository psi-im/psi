#!/bin/sh

for f in `find botan -name \*.cpp -o -name \*.h` ; do
	./wrapns $f QCA
	#sed -e 's/std::/::std::/g' $f > file.tmp
	#cp file.tmp $f
	#rm file.tmp
done

