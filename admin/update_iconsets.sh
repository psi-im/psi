#!/bin/sh

SOURCE_DIR=../../iconsets
TARGET_DIR=../iconsets

ROSTER_DEFAULT='crystal-roster'
ROSTER_EXTRAS='
	crystal-aim crystal-icq crystal-msn crystal-service crystal-yahoo 
	crystal-gadu crystal-sms crystal-roster
	'

SYSTEM_DEFAULT='crystal-system'
SYSTEM_EXTRAS=''

################################################################################

if test ! -d $SOURCE_DIR; then
	echo "Cannot find source dir $SOURCE_DIR"
	exit
fi

if test ! -d $TARGET_DIR; then
	echo "Cannot find target dir $TARGET_DIR"
	exit
fi

################################################################################

# Roster iconsets
echo '*** Updating Roster iconsets ***'
#cp -R $SOURCE_DIR/roster/$ROSTER_DEFAULT/* $TARGET_DIR/roster/default
rm -f $TARGET_DIR/roster/default/Makefile
for i in $ROSTER_EXTRAS; do
	make -C $SOURCE_DIR/roster $i.jisp
	cp $SOURCE_DIR/roster/$i.jisp $TARGET_DIR/roster
done

################################################################################

# System iconsets
echo '*** Updating System iconsets ***'
#cp -R $SOURCE_DIR/system/$SYSTEM_DEFAULT/* $TARGET_DIR/system/default
rm -f $TARGET_DIR/system/default/Makefile
for i in $SYSTEM_EXTRAS; do
	make -C $SOURCE_DIR/system $i.jisp
	cp $SOURCE_DIR/system/$i.jisp $TARGET_DIR/system
done
