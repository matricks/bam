#!/usr/bin/env python

import re, time

class Node:
	def __init__(self, name):
		self.name = name
		self.body = ""
		self.index = ""
		self.indexname = ""
		self.tag = ""
		self.nodes = []

# tags
group_tag = "@GROUP"
function_tag = "@FUNCTION"
option_tag = "@OPTION"
body_tag = "@BODY"
tags = [function_tag, option_tag, body_tag]
end_tag = "@END"

class Output:
	def render_node_index(self, cur):
		if len(cur.index):
			print >>self.file, self.index_node_begin(cur)
		for node in cur.nodes:
			self.render_node_index(node)
		if len(cur.index):
			print >>self.file, self.index_node_end(cur)
	def render_node(self, cur):
		if len(cur.index):
			print >>self.file, self.format_header(cur)
			print >>self.file, self.format_body(cur)
		for node in cur.nodes:
			self.render_node(node)
			
	def index_nodes(self, cur, index=""):
		for i in xrange(0, len(cur.nodes)):
			if len(index):
				cur.nodes[i].index = index + "." + str(i+1)
			else:
				cur.nodes[i].index = str(i+1)
			
			cur.nodes[i].indexname = cur.nodes[i].name
			if cur.nodes[i].tag == function_tag:
				cur.nodes[i].indexname = cur.nodes[i].indexname.split("(")[0].strip()
			self.index_nodes(cur.nodes[i], cur.nodes[i].index)
		
	def render(self, rootnode):
		self.index_nodes(rootnode)
		print >>self.file, self.render_begin()
		
		print >>self.file, self.index_begin()
		self.render_node_index(rootnode)
		print >>self.file, self.index_end()
		
		self.render_node(rootnode)
		print >>self.file, self.render_end()

class HTMLOutput(Output):
	def output_name(self):
		return "bam.html"
		
	def render_begin(self):
		# large part of the style sheet is stolen from luas documentation
		return '''
			<head>
			<title>Bam Manual</title>
			
			<style type="text/css"><!--
			
				body {
					color: #000000 ;
					background-color: #FFFFFF ;
					font-family: sans-serif ;
					text-align: justify ;
					margin-right: 20px ;
					margin-left: 20px ;
				}

				h1, h2, h3, h4, h5 {
					font-weight: normal ;
					font-style: italic ;
				}

				a:link {
					color: #000080 ;
					background-color: inherit ;
					text-decoration: none ;
				}

				a:visited {
					background-color: inherit ;
					text-decoration: none ;
				}

				a:link:hover, a:visited:hover {
					color: #000080 ;
					background-color: #E0E0FF ;
				}

				a:link:active, a:visited:active {
					color: #FF0000 ;
				}

				hr {
					border: 0 ;
					height: 1px ;
					color: #a0a0a0 ;
					background-color: #a0a0a0 ;
				}
				
				pre {
					padding: 5px ;
					background-color: #eeeeee ;
				}
			
				.identifier {
					font-family: monospace ;
				}
			--></style>
			</head>
			<body>
			<hr/>
			<img src="bam_logo.png"/><h1>Bam Manual</h1>
			<small>This manual is a work in progress and is not complete.</small>
			
			<hr/>
			Copyright &copy; 2008 Magnus Auvinen. Freely available under the terms of the zlib/libpng license.
		'''
	def render_end(self):
		return '''
			<hr/>
			<small>Generated at %s.</small>
			</body>
		''' % (time.asctime())

	def index_begin(self): return '<h2>Contents</h2><ul>'
	def index_end(self): return '</ul><hr/>'
	def index_node_begin(self, node):
		return '<li><a href="#%s">%s - %s</a></li><ul>'%(node.index,node.index,node.indexname)
	def index_node_end(self, node):
		if len(node.index) == 1:
			return '</ul><p></p>'
		return '</ul>'
	
	def format_header(self, node):
		i = (len(node.index)-1)/2 + 1
		header = '<h%d><a name="%s"></a><a name="%s">%s</a> - <a name="%s">%s</a></h%d>'%(i,node.indexname,node.index,node.index,node.name,node.name,i)
		if node.tag == function_tag:
			header = '<hr/>' + header
		return header

	def format_body(self, node):
		body = node.body
		body = re.sub('\^(?P<ident>[^\^]+)\^', '<span class="identifier">\g<ident></span>', body)
		body = re.sub('\[(?P<ident>[^\]]+)\]', '<a href="#\g<ident>">\g<ident></a>', body)
		body = re.sub('{{{{', '<pre>', body)
		body = re.sub('}}}}', '</pre>', body)
		body = re.sub('\n\n', '</p><p>', body)
		body = '<p class="body">' + body + '</p>\n'
		return body

outputs = [HTMLOutput()]

def ParseTextFile(rootnode, filename, addbr=False):
	group = rootnode
	for line in file(filename):
		if group_tag in line:
			group_name = line.split(group_tag)[-1].split(end_tag)[0].strip()
			group = Node(group_name)
			rootnode.nodes += [group]
		else:
			if addbr:
				group.body += line.strip() + "<br/>\n"
			else:
				group.body += line.strip() + "\n"
			
	return rootnode

def ParseFile(rootnode, filename):
	# 0 = scaning for start tag
	# 1 = scaning for end tag,
	# 2 = outputting function decl
	state = 0
	group = rootnode
	for line in file(filename):
		if state == 0:
			if group_tag in line:
				group_name = line.split(group_tag)[-1].split(end_tag)[0].strip()
				group = Node(group_name)
				rootnode.nodes += [group]
			else:
				for t in tags:
					if t in line:
						title = line.split(t)[-1].strip()
						tag = t
						body = ""
						state = 1
						break
		elif state == 1:
			if end_tag in line:
				state = 2
			else:
				body += line.strip() + "\n"
		else:
			if tag == function_tag:
				if len(title) == 0:
					title = line.replace("function", "").strip()
				title = title.replace("(", " (")
			node = Node(title)
			node.body = body
			node.tag = tag
			group.nodes += [node]
			state = 0
	return rootnode

# parse files
group = 0
root = Node("root")
root.nodes += [ParseTextFile(Node("Introduction"), "docs/introduction.txt")]
root.nodes += [ParseTextFile(Node("Building Bam"), "docs/building.txt")]
root.nodes += [ParseTextFile(Node("Quick Start (TODO)"), "docs/quickstart.txt")]
root.nodes += [ParseTextFile(Node("Custom Actions"), "docs/actions.txt")]
root.nodes += [ParseFile(Node("Command Line Reference (TODO)"), "src/main.c")]
root.nodes += [ParseFile(Node("Function Reference (TODO)"), "src/base.bam")]
root.nodes += [Node("Settings Reference (TODO)")]
root.nodes += [ParseTextFile(Node("License"), "license.txt", True)]
#notes.nodes +=[Node("C/C++ Dependency Checker")]
#notes.nodes +=[Node("Spaces in Paths")]

# render files
for o in outputs:
	o.file = file("docs/"+o.output_name(), "w")
	o.render(root)
	o.file.close()
