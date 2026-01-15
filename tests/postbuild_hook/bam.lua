
function MyPostBuildHook()
	print("MyPostBuildHook:")
	assert(Exist( "output1"))
	assert(IsOutput("output1"))
	assert(IsOutput("output3") )
	assert(IsTargeted("output1") )
	assert(not IsTargeted("output3") )
	assert(ScriptArgs["posthook_test_fail"] == "0")
end

function MyPostBuildHook_Overwritten()
	error("MyPostBuildHook_Overwritten: this one should not run")
end

if ScriptArgs["posthook_test_fail"] ~= "0" then
	print("post build test: should fail")
else
	print("post build test: should pass")
end

AddJob({"output1", "output2"}, "testing 1", "echo hello > output1 && echo world >output2")
AddJob({"output3"}, "testing 3", "echo hello > output3")

DefaultTarget("output1")

SetPostBuildHook(MyPostBuildHook_Overwritten)
SetPostBuildHook(MyPostBuildHook)