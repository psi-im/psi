#!/usr/bin/python

from xml.dom.minidom import parse, parseString
import xml.dom
import sys

def rec_parse(node, context): # node : xml.dom.Node
	for i in node.childNodes:
		if i.nodeType == xml.dom.Node.ELEMENT_NODE:
			if i.hasAttribute("comment"):
				print 'QT_TRANSLATE_NOOP("' + context + '","' + i.getAttribute("comment") + '");';
			rec_parse(i,context)


if len(sys.argv) != 2:
	print "usage: %s options.xml > output.cpp" % sys.argv[0]
	sys.exit(1)

print "#define QT_TRANSLATE_NOOP(a,b)"

dom = parse(sys.argv[1]) # parse an XML file by name

toplevel = dom.getElementsByTagName("psi")[0]
options = toplevel.getElementsByTagName("options")[0]

shortcuts = options.getElementsByTagName("shortcuts")[0]
rec_parse(shortcuts,"Shortcuts")
