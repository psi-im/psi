#!/bin/sh

if [ $# != 2 ]; then
	echo "usage: $0 [url] [file]"
	exit 1
fi

WGET=`which wget`
if [ ! -z "$WGET" ]; then
	echo $WGET -O $2 $1
	$WGET -O $2 $1
else
	CURL=`which curl`
	if [ ! -z "$CURL" ]; then
		echo $CURL -o $2 $1
		$CURL -o $2 $1
	else
		echo "error: need wget or curl in path"
		exit 1
	fi
fi
