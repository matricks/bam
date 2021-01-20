
-- similar to cxx_dep test but with where the same c-file is built with different include paths, resulting in differnt used header

function genheader(path)
	local label = "headergen " .. path
	if family == "windows" then
		return AddJob(Path(path), label, "echo /**/ >" ..  path)
	else
		return AddJob(Path(path), label, "echo \"/**/\" > " .. path)
	end
end

exes = {}
for i = 1,10,1 do
	s = NewSettings()
	s.cc.includes:Add("dir")
	s.cc.includes:Add("dir2")
	
	-- some differnt variants of paths
	local genheaderbase = "genh/" .. i
	if i < 4 and family == "windows" then
		genheaderbase = "genh\\" .. i --and a backwards one on windows
	elseif i < 6 then
		genheaderbase = "genh_2//herp/" .. i
	end
		
	s.cc.includes:Add(genheaderbase)
	s.cc.includes:Add("dir3")
	s.cc.Output = function( settings, input )
		return "build/imm_/" .. i .. "/" .. PathBase(input) .. settings.config_ext
	end

	obj1 = Compile(s, "src.c")
	objm = Compile(s, "main.c")
	res = Link(s, "build/ouput_" .. i .. "/dep_cxx", objm, obj1)
	table.insert(exes, res)
	
	genheader(genheaderbase .. "/genereted_header.h")
end

DefaultTarget(PseudoTarget( "fancy_main_target", exes ))
