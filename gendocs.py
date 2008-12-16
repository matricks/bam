import re


class Node:
	def __init__(self, name):
		self.name = name
		self.body = ""
		self.index = ""
		self.tag = ""
		self.nodes = []

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
			print >>self.file, self.format_header(cur.index, cur.name)
			print >>self.file, self.format_body(cur.body)
		for node in cur.nodes:
			self.render_node(node)
			
	def index_nodes(self, cur, index=""):
		for i in xrange(0, len(cur.nodes)):
			if len(index):
				cur.nodes[i].index = index + "." + str(i+1)
			else:
				cur.nodes[i].index = str(i+1)
			
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

				h1, h2, h3, h4 {
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

			
				.identifier {
					font-family: monospace ;
				}
			--></style>
			</head>
			<body>
			<hr/>
			<img src="bam_logo.png"/><h1>Bam Manual</h1>
			<hr/>
		'''
	def render_end(self):
		return '''
			<hr/>
			</body>
		'''

	def index_begin(self): return '<h2>Contents</h2>'
	def index_end(self): return '<hr/>'
	def index_node_begin(self, node):
		name = node.name
		if node.tag == function_tag:
			name = name.split("(")[0].strip()
		return '<a href="#%s">%s - %s</a><ul>'%(node.index,node.index,name)
	def index_node_end(self, node): return '</ul>'
	
	def format_header(self, index, name):
		i = (len(index)-1)/2 + 2
		return '<h%d><a name="%s">%s - %s</a></h%d>'%(i,index,index,name,i)
	def format_body(self, body):
		body = re.sub('\^(?P<ident>(\w)+)', '<span class="identifier">\g<ident></span>', body)
		return '<p class="body">' + body + '</p>\n'

class WikiOutput(Output):
	def output_name(self):
		return "bam.txt"
	def format_header(self, decl):
		return "== ''!" + decl + "'' =="
	def format_body(self, body):
		body = re.sub('\^(?P<ident>(\w)+)', "{{{\g<ident>}}}", body)
		return body + "\n----"

outputs = [HTMLOutput()] #, WikiOutput()]

# tags
group_tag = "@GROUP"
function_tag = "@FUNCTION"
option_tag = "@OPTION"
tags = [function_tag, option_tag]
end_tag = "@END"

root = Node("root")
function_reference = Node("Function Reference")
cli_reference = Node("Command Line Reference")
group = 0
root.nodes += [Node("Introduction")]
root.nodes += [Node("Building Bam")]
root.nodes += [Node("Quick Start")]
root.nodes += [Node("Custom Actions")]
root.nodes += [cli_reference]
root.nodes += [function_reference]
root.nodes += [Node("Settings Reference")]
notes = Node("Implementation Notes")
root.nodes += [notes]
notes.nodes +=[Node("C/C++ Dependency Checker")]
notes.nodes +=[Node("Spaces in Paths")]

def parse_file(rootnode, filename):
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
				body += line.strip() + " "
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

# parse files
parse_file(function_reference, "src/base.bam")
parse_file(cli_reference, "src/main.c")

# render files
for o in outputs:
	o.file = file("docs/"+o.output_name(), "w")
	o.render(root)
	o.file.close()
