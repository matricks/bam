settings = NewSettings() 

AddJob("header_gen.h", "headergen", "touch header_gen.h") 
src = Collect("*.c") 
objs = Compile(settings, src) 
exe = Link(settings, "deadlock", objs) 
