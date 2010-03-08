settings = NewSettings() 

if family == "windows" then
	AddJob("header_gen.h", "headergen", "echo /**/ > header_gen.h") 
else
	AddJob("header_gen.h", "headergen", "echo \"/**/\" > header_gen.h") 
end
src = Collect("*.c") 
objs = Compile(settings, src) 
exe = Link(settings, "deadlock", objs) 
