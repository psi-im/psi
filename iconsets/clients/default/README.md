To add the icon edit the following files:
client_icons.txt
iconsets.qrc.in
iconsets/clients/default/icondef.xml


Next command was used to generate png from svg

```
for f in ~/projects/psi/svg/clients/default/*.svg; do convert -background none -resize 16x16 -define png:compression-level=9 -define png:compression-strategy=1 -depth 24 -define png:compression-filter=2 +antialias $f $(basename $f .svg).png; done
```
