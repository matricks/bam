AddJob({"output1", "output2"}, "testing 1", "echo hello > output1 && echo world >output2")

AddJob("output3", "testing 2", "echo hello > output3 && echo world >output4")
AddOutput("output3", "output4")
