
function QuickJob(name, deps)
	AddJob(name, name, "echo > " .. name, deps)
end

function QuickJob2x(name0, name1, deps)
	AddJob({name0, name1}, "2x " .. name0 .. " " .. name1,  "echo > " .. name0 .. " && echo > " .. name1, deps)
end

QuickJob("A_common2", {})
QuickJob("A_common1", {"A_common2"})
QuickJob("A_a", {"A_common1"})
QuickJob("A_b", {"A_common1"})
ModifyPriority("A_a", 100)

QuickJob("B_common2", {})
QuickJob("B_common1", {"B_common2"})

-- multi output is tricky here as multiple nodes references the same job-struct
QuickJob2x("B_a", "B_b", {"B_common1", "B_common1"})
ModifyPriority("B_a", 100)

QuickJob("C", {"A_common2", "B_common1"})

QuickJob("D", {"B_a"})

QuickJob("top", {"A_a", "A_b", "B_b", "B_a", "D"}) -- order of deps should not affect priorities
DefaultTarget("top")
