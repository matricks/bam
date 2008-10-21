#!/usr/bin/python

import os, sys

def get_symbols(f):
	lines = os.popen("objdump -t "+f).readlines()
	exports = []
	imports = []

	state = 0
	for line in lines:
		if state:
			try:
				address = line[0:8]
				scope = line[9]
				l = line[17:].split(" ")
				location = l[0].split("\t")[0]
				name = l[1].strip()
				if location == "*UND*":
					imports += [name]
				elif scope == "g":
					exports += [name]
			except:
				pass
		else:
			if line.strip() == "SYMBOL TABLE:":
				state = 1
	return imports,exports	

files = []
for filename in os.listdir("src"):
	if filename[-2:] == ".o":
		files += [filename]

all_exports = []
all_imports = []
info = {}

for f in files:
	imports,exports = get_symbols(f)
	all_exports += exports
	all_imports += imports
	for e in exports:
		info[e] = f
	
for export in all_exports:
	if export in all_imports:
		pass
	else:
		print info[export]+": "+ export
