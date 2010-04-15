s = NewSettings()
s.cc.includes:Add("dir")
s.link.libpath:Add("libs")
s.link.libs:Add("hello") -- the look up for this lib should be deferred
DefaultTarget(Link(s, "dep_cxx", Compile(s, "main.c")))

if family == "windows" then
	AddJob("dir/genereted_header.h", "headergen", "echo /**/ > dir/genereted_header.h") 
else
	AddJob("dir/genereted_header.h", "headergen", "echo \"/**/\" > dir/genereted_header.h") 
end

StaticLibrary(s, "libs/hello", Compile(s, "lib.c"))
