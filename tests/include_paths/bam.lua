s = NewSettings()
s.cc.includes:Add("dir")
Link(s, "include_paths", Compile(s, Collect("*.c")))
