settings = NewSettings() 
 
src = Collect("*.cpp") 
objs = Compile(settings, src) 
exe = SharedLibrary(settings, "shared", objs) 
