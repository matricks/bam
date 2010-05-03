s = NewSettings()
e = Link(s, "deps", Compile(s, "main.c"))
AddDependency(e, "DOES_NOT_EXIST")
