-- this is a test for a bug where job deps where not forfilled for multi-output jobs when you only dependended on one of the non-primary outputs.
-- bam would run the job without running the main job node deps. In this test we have a job creating output_source1, and then a multi-output job 
-- with output3 as primary and then output4 as secondary outputs. The primary node is dependent on output_source1, but we only depend on the 
-- second output. Second case is with all outputs added in AddJob, that case works. Third is adding the deps after all the outputs are added,
-- behaves like the first

local cat_cmd = ''
if family == 'windows' then
	cat_cmd = 'type'
else 
	cat_cmd = 'cat'
end

-- case 1 (output added after)
AddJob("output_source_1", "testing source 1", "echo hello source 1 > output_source_1")

AddJob("output_1_1", "testing 2", cat_cmd .. " output_source_1 > output_1_1 && " .. cat_cmd .. " output_source_1 >output_1_2", "output_source_1" )
AddOutput("output_1_1", "output_1_2")


-- case 2 (output added inline)
AddJob("output_source_2", "testing source 2", "echo hello source 1 > output_source_2")

AddJob({"output_2_1", "output_2_2"}, "testing 2", cat_cmd .. " output_source_2 > output_2_1 && " .. cat_cmd .. " output_source_2 >output_2_2", "output_source_2" )


-- case 3 (deps added last)
AddJob("output_source_3", "testing source 3", "echo hello source 1 > output_source_3")

AddJob({"output_3_1", "output_3_2"}, "testing 3", cat_cmd .. " output_source_3 > output_3_1 && " .. cat_cmd .. " output_source_3 >output_3_2" )
AddDependency( "output_3_1", "output_source_3" )


PseudoTarget( 'test_target', {"output_1_2", "output_2_2", "output_3_2" } )
--PseudoTarget( 'test_target', {"output_1_2"} )
--PseudoTarget( 'test_target', {"output_2_2"} )
--PseudoTarget( 'test_target', {"output_3_2"} )

DefaultTarget("test_target")