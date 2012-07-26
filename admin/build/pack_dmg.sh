#!/bin/sh
set -e

if [ $# != 3 ]; then
	echo "usage: $0 [dmg file] [volume name] [content dir]"
	exit 1
fi

VOLUME_NAME=$2
DISK_DIR=$3
WC_DMG=wc.dmg
WC_DIR=wc
TEMPLATE_DMG=template.dmg
MASTER_DMG=$1

# generate empty template
if [ ! -f "$TEMPLATE_DMG.bz2" ]; then
	echo generating empty template
	mkdir template
	hdiutil create -size 160m "$TEMPLATE_DMG" -srcfolder template -format UDRW -volname "$VOLUME_NAME" -quiet
	rmdir template
	bzip2 "$TEMPLATE_DMG"
fi

if [ ! -f "$TEMPLATE_DMG" ]; then
	bunzip2 -k $TEMPLATE_DMG.bz2
fi

# fill in with psi
echo making psi dmg

cp $TEMPLATE_DMG $WC_DMG

mkdir -p $WC_DIR
hdiutil attach "$WC_DMG" -noautoopen -quiet -mountpoint "$WC_DIR"
cp -a $DISK_DIR/* $WC_DIR
diskutil eject `diskutil list | grep "$VOLUME_NAME" | grep "Apple_HFS" | awk '{print $6}'`

rm -f $MASTER_DMG
hdiutil convert "$WC_DMG" -quiet -format UDZO -imagekey zlib-level=9 -o $MASTER_DMG
rm -rf $WC_DIR $WC_DMG
hdiutil internet-enable -yes -quiet $MASTER_DMG || true
