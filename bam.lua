CheckVersion("0.3")

config = NewConfig()
config:Add(OptCCompiler("cc"))
config:Finalize("config.bam")

s = config:NewSettings()

s.debug = 1
s.optimize = 0

if s.cc.c_compiler == "gcc" then
	s.cc.flags:Add("-Wall", "-ansi", "-pedantic", "-O2")
	s.link.libs:Add("pthread")
end

if s.cc.c_compiler == "cl" then

end

txt2c_tool = Link(s, "src/tools/txt2c", Compile(s, "src/tools/txt2c.c"))

base_files = {"src/base.lua", "src/driver_gcc.lua", "src/driver_cl.lua"}
internal_base = "src/internal_base.h"
AddJob(
	internal_base, 
	"embedding lua code",
	"src/tools/txt2c " .. tbl_to_str(base_files, " ", "") .. " > "..internal_base,
	txt2c_tool, base_files)

s.cc.includes:Add("src/lua")
bam = Link(s, "bam", Compile(s, Collect("src/*.c", "src/lua/*.c")))
DefaultTarget(bam)
