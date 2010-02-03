settings = NewSettings() 

AddJob("test", "testing", "echo")
AddDependency("test", "test2") 
AddJob("test2", "testing", "echo")

--src = Collect("*.cpp") 
--objs = Compile(settings, src) 
--exe = Link(settings, "addorder", objs) 
