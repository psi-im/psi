#!/bin/sh

for f in `find botan -name \*.cpp -o -name \*.h` ; do
	./wrapns $f QCA
done

