
function QuickJob(name, deps)
	AddJob(name, name, "echo > " .. name, deps)
end

QuickJob("A_common2", {})
QuickJob("A_common1", {"A_common2"})
QuickJob("A_a", {"A_common1"})
QuickJob("A_b", {"A_common1"})
ModifyPriority("A_a", 100)

QuickJob("B_common2", {})
QuickJob("B_common1", {"B_common2"})
QuickJob("B_a", {"B_common1"})
QuickJob("B_b", {"B_common1"})
ModifyPriority("B_a", 100)

QuickJob("C", {"A_common2", "B_common1"})

QuickJob("top", {"A_a", "A_b", "B_b", "B_a"})
DefaultTarget("top")
