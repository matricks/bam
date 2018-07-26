settings = NewSettings() 

AddJob("test", "testing", "echo > test")
AddDependency("test", "test2") 
AddJob("test2", "testing", "echo > test2")

--src = Collect("*.cpp") 
--objs = Compile(settings, src) 
--exe = Link(settings, "addorder", objs) 
