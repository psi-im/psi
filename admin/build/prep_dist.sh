#!/bin/sh
set -e

if [ $# != 3 ]; then
	echo "usage: $0 [destdir] [prefix] [distdir]"
	exit 1
fi

platform=`uname -s`
if [ "$platform" == "Darwin" ]; then
	platform=mac
elif [ "$platform" == "MINGW32_NT-6.1" ]; then
	platform=win
else
	echo "error: unsupported platform $platform"
	exit 1
fi

#destdir=$1
destdir=
base_prefix=$2
dist_base=$3

mkdir -p $dist_base

if [ "$platform" == "mac" ]; then
	target_base=$destdir$base_prefix
	target_dist_base=$dist_base

	mkdir -p $target_dist_base
	cp -a $target_base/bin $target_dist_base
	cp -a $target_base/lib $target_dist_base
	cp -a $target_base/plugins $target_dist_base

	cp -a distfiles/mac/README $dist_base
else
	TARGET_ARCHES="i386 x86_64"

	for target_arch in $TARGET_ARCHES; do
		target_base=$destdir$base_prefix/$target_arch
		target_dist_base=$dist_base/$target_arch

		mkdir -p $target_dist_base
		cp -a $target_base/bin $target_dist_base
		cp -a $target_base/include $target_dist_base
		cp -a $target_base/lib $target_dist_base
		cp -a $target_base/plugins $target_dist_base
	done

	cp -a distfiles/win/README $dist_base
fi
