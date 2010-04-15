s = NewSettings()
s.cc.includes:Add("dir")
DefaultTarget(Link(s, "dep_cxx", Compile(s, Collect("*.c"))))

if family == "windows" then
	AddJob("dir/genereted_header.h", "headergen", "echo /**/ > dir/genereted_header.h") 
else
	AddJob("dir/genereted_header.h", "headergen", "echo \"/**/\" > dir/genereted_header.h") 
end
