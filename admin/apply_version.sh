#!/bin/bash

version=$1

# this script stamps a version to files that need a version before the build
#   process has started. everything else gets versionized at configure time or
#   later.

# rewrite version in README
line1="Psi $version"
line2=
len1=${#line1}
n=0
while [ $n -lt $len1 ]; do
	line2="$line2-"
	n=`expr $n + 1`
done
echo $line1 > README.new
echo $line2 >> README.new
tail -n+3 README >> README.new
mv README.new README
