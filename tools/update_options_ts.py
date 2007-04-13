#!/usr/bin/python

from xml.dom.minidom import parse, parseString
import xml.dom


def rec_parse(node): # node : xml.dom.Node
	for i in node.childNodes:
		if i.nodeType == xml.dom.Node.ELEMENT_NODE:
			if i.hasAttribute("comment"):
				print 'QT_TRANSLATE_NOOP("' + i.getAttribute("comment") + '");';
			rec_parse(i)


dom = parse('options/default.xml') # parse an XML file by name

toplevel = dom.getElementsByTagName("psi")[0]
options = toplevel.getElementsByTagName("options")[0]
shortcuts = options.getElementsByTagName("shortcuts")[0]

rec_parse(shortcuts)
