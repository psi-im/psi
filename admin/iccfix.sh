#!/bin/sh

find ./ -name '*.png' -exec pngout -kiCCP,gAMA '{}' \;
for f in ./iconsets/roster/*.jisp; do isn=$(basename $f .jisp); f=$(readlink -f $f); ( mkdir ~/temp/is; cd ~/temp/is; unzip $f; for p in $isn/*.png; do pngout -kiCCP,gAMA $p; done; zip -r9 $f.x * && mv $f.x $f || echo "Failed to zip" ; rm -rf ~/temp/is; ) ;  done
