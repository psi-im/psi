import xml.parsers.expat, sys,traceback

class xmlobj(object):

	def __init__(self):
		self.name = ''
		self.cdata = ''
		self.attrs = {}
		self.subs = [] # list of subs
		self.subnames = {} # 2nd way to access subs
		self.parent = None
		self.index = -1

	def __getattr__(self, name):
		return self.subnames.get(name, None)
	
	def sub(self, name):
		return self.subnames.get(name, None)
	
	def __getitem__(self, key):
		if key == '_cdata':
			return self.cdata
		return self.attrs.get(key,'')
	
	def __iter__(self):
		return self
	
	def next(self):
		self.index += 1
		if self.index > len(self.subs) - 1:
			raise StopIteration, "Looped through all sub elements."
		return self.subs[self.index]
	
	def getxml(self, depth = 0):
		tab = '\t' * depth
		attrstring = ''
		for attr_name in self.attrs:
			attrstring += ' %s="%s"' % (attr_name, self.attrs[attr_name])
		xmlstring = "%s<%s%s" % (tab, self.name, attrstring)
		if self.cdata == '' and self.subs == []:
			xmlstring += '/>'
		else:
			xmlstring += '>'
			if self.cdata != '':
				xmlstring += '\n%s' % self.cdata
			for sub in self.subs:
				xmlstring += "\n" + sub.getxml(depth + 1)
			xmlstring += "\n%s</%s>" % (tab,self.name)
		return xmlstring

class xmllib(object):
	def __init__(self, xml_file, type='string'):
		self.parser = xml.parsers.expat.ParserCreate()
		self.parser.CharacterDataHandler = self.h_data
		self.parser.StartElementHandler = self.h_se
		self.parser.EndElementHandler = self.h_ee
		self.root = xmlobj()
		self.element = self.root
		try:
			if type == 'file':
				self.parser.ParseFile(open(xml_file, 'r'))
			elif type == 'string':
				self.parser.Parse(xml_file)
		except xml.parsers.expat.ExpatError:
			#exception, tb,rowcol =  traceback.format_exc().split('\n')[-2].split(':',3)
			##print "-" * 80
			#print "XML Error!\n%s on %s\n%s" % (xml_file, rowcol.strip(), tb.strip())
			#print "-" * 80
			print "Expat Parsing Error."
			sys.exit()
	
	def h_se(self, name, attrs):
		newelement = xmlobj() # start a new element
		newelement.name = name # name it
		newelement.attrs = attrs # assign attributes
		newelement.parent = self.element # assign the current element as it's parent
		self.element.subs.append(newelement) # apparend to subs of the current element
		# appending subname
		if not self.element.subnames.has_key(name):
			self.element.subnames[name] = [] 
		self.element.subnames[name].append(newelement) 
		# ----------------
		self.element = newelement # assign the new element as the current one
		
	def h_data(self, data):
		self.element.cdata += data # assign cdata
	
	def h_ee(self, name):
		self.element.cdata = self.element.cdata.strip()
		self.element = self.element.parent # element is done, set current to parent element
	
	def getroot(self):
		return self.root.subs[0]

def loadfile(file):
	xmlparser = xmllib(file)
	return xmlparser.getroot()

if __name__ == '__main__':
	message = """<message to='user@gmail.com' from='john@doe.com' id='abc123'>
	<subject>Google Federated!</subject>
	<body>I can totally send you messages from gmail now!</body>
</message>"""
	xmlparser = xmllib(message)
	xml = xmlparser.getroot()
	for sub in xml:
		print sub.name
		print sub['_cdata']
	print xml.sub('body')[0]['_cdata']
	print xml['from']
