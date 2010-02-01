settings = NewSettings() 

AddJob("test", "testing", "pwd")
AddDependency("test", "test2") 
AddJob("test2", "testing", "pwd")

--src = Collect("*.cpp") 
--objs = Compile(settings, src) 
--exe = Link(settings, "addorder", objs) 
