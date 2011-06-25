settings = NewSettings() 

settings.cc.flags:Add("-fPIC")
 
src = Collect("*.cpp") 
objs = Compile(settings, src) 
exe = SharedLibrary(settings, "shared", objs) 
