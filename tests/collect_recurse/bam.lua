settings = NewSettings() 
 
src = CollectRecursive("*.cpp")
objs = Compile(settings, src) 
exe = Link(settings, "output/creation/gc_app", objs) 
