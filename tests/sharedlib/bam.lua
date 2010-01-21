settings = NewSettings() 

if arch == "amd64" then
	settings.cc.flags:Add("-fPIC")
end
 
src = Collect("*.cpp") 
objs = Compile(settings, src) 
exe = SharedLibrary(settings, "shared", objs) 
