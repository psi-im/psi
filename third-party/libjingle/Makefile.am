SUBDIRS=talk

EXTRA_DIST=README.win DOCUMENTATION

dist-hook:
	sed -i -f talk/sanitize.sed `find $(distdir) -type f`
