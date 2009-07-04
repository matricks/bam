s = NewSettings()
objs = Compile(s, Collect("*.c"))
PseudoTarget("CORRECT_ONE", Link(s, "correct", objs))
Link(s, "ERROR", objs)
