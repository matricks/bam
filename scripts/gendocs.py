#!/usr/bin/env python
from tinydoc import *
import os

os.system("dot -Tpng docs/depgraph.dot > docs/depgraph.png")


info = DocInfo()
info.name = "Bam Manual"
info.logo = "bam_logo.png"
info.note = "This manual is a work in progress and is not complete."
info.copyright = "Copyright &copy; 2008 Magnus Auvinen. Freely available under the terms of the zlib/libpng license."

outputs = [HTMLOutput("docs/bam.html", info)]

group = 0
root = Node("root")
root.nodes += [ParseTextFile(Node("Introduction"), "docs/introduction.txt")]
root.nodes += [ParseTextFile(Node("Building Bam"), "docs/building.txt")]
root.nodes += [ParseTextFile(Node("Quick Start (TODO)"), "docs/quickstart.txt")]
root.nodes += [ParseTextFile(Node("Custom Actions"), "docs/actions.txt")]
root.nodes += [ParseFile(Node("Command Line Reference (TODO)"), "src/main.c")]
root.nodes += [ParseFile(Node("Function Reference (TODO)"), "src/base.lua")]
root.nodes += [Node("Settings Reference (TODO)")]
root.nodes += [ParseTextFile(Node("License"), "license.txt", True)]
#notes.nodes +=[Node("C/C++ Dependency Checker")]
#notes.nodes +=[Node("Spaces in Paths")]

# render files
for o in outputs:
	o.file = file(o.output_name(), "w")
	o.render(root)
	o.file.close()
