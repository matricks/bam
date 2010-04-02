function SetDefaultDrivers(settings)
	-- set default drivers
	if family == "windows" then
		SetDriversCL(settings)
	else
		SetDriversGCC(settings)
	end
end

------------------------ C/C++ COMPILE ------------------------

function DriverNull()
	error("no driver set")
end

function InitCommonCCompiler(settings)
	if settings.cc then return end
	
	settings.cc = {}
	settings.cc._c_cache = { nr = 0, str = "" }
	settings.cc._cxx_cache = { nr = 0, str = "" }
	settings.cc.extension = ""
	settings.cc.c_exe = ""
	settings.cc.cxx_exe = ""
	settings.cc.DriverCTest = DriverNull
	settings.cc.DriverC = DriverNull
	settings.cc.DriverCXX = DriverNull
	settings.cc.flags = NewFlagTable()
	settings.cc.c_flags = NewFlagTable()
	settings.cc.cpp_flags = NewFlagTable()
	settings.cc.includes = NewTable()
	settings.cc.systemincludes = NewTable()
	settings.cc.defines = NewTable()
	settings.cc.frameworks = NewTable()
	settings.cc.Output = Default_Intermediate_Output
	settings.cc.optimize = 0

	TableLock(settings.cc)
end

function CCompiler(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	settings.cc.DriverC(settings.labelprefix .. "c " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_dependency_cpp(input, settings.cc.includes)
	return outname
end

function CXXCompiler(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	settings.cc.DriverCXX(settings.labelprefix .. "c++ " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_dependency_cpp(input, settings.cc.includes)	
	return outname
end


AddTool("c", function (settings)
	InitCommonCCompiler(settings)
	settings.filemappings["c"] = CCompiler
	settings.filemappings["m"] = CCompiler
end)

AddTool("cxx", function (settings)
	InitCommonCCompiler(settings)
	settings.filemappings["cpp"] = CXXCompiler
	settings.filemappings["cxx"] = CXXCompiler
	settings.filemappings["c++"] = CXXCompiler
	settings.filemappings["cc"] = CXXCompiler
end)

------------------------ COMPILE ACTION ------------------------
	
--[[@GROUP Compile @END]]--
-- Compiles C, Obj-C and C++ files
--[[@UNITTESTS
	err=1; find="expected a settings object": Compile(nil)
	err=1; find="compiler returned a nil": s = NewSettings(); s.filemappings["c"] = function()end; Compile(s, "test.c")
	err=0 : s = NewSettings(); Compile(s)
@END]]--
--[[@FUNCTION
	TODO
@END]]--
function Compile(settings, ...)
	CheckSettings(settings)
	local outputs = {}
	
	settings.invoke_count = settings.invoke_count + 1

	for inname in TableWalk({...}) do
		-- fetch correct compiler
		local ext = PathFileExt(inname)
		local Compiler = settings.filemappings[ext]

		if not Compiler then
			error("'"..inname.."' has unknown extention '"..ext.."' which there are no compiler for")
		end
		
		local objectfile = Compiler(settings, inname)
		if not IsString(objectfile) then
			error("compiler returned a "..type(objectfile).." instead of a string")
		end
		table.insert(outputs, objectfile)
	end
	
	-- return the output
	return outputs	
end

function CTestCompile(settings, code, options)
	return settings.cc.DriverCTest(code, options)
end

--[[@GROUP Other @END]]--
--[[@FUNCTION
	e
@END]]--
function Copy(outputdir, ...)
	local outputs = {}

	local copy_command = "cp"
	local copy_append = ""

	if family == "windows" then
		copy_command = "copy /b" -- binary copy
		copy_append = " >nul 2>&1" -- suppress output
	end
	
	-- compile all the files
	for inname in TableWalk({...}) do
		output = Path(outputdir .. "/" .. PathFilename(inname))
		input = Path(inname)

		local srcfile = input
		local dstfile = output
		if family == "windows" then
			srcfile = str_replace(srcfile, "/", "\\")
			dstfile = str_replace(dstfile, "/", "\\")
		end

		AddJob(output,
			"copy " .. input .. " -> " .. output,
			copy_command .. " " .. srcfile .. " " .. dstfile .. copy_append)

		-- make sure that the files timestamps are updated correctly
		SetTouch(output)
		AddDependency(output, input)
		table.insert(outputs, output)
	end
	
	return outputs
end


------------------------ LINK ------------------------

--[[@GROUP Link @END]]--

AddTool("link", function (settings)
	settings.link = {}
	settings.link.Driver = DriverNull
	settings.link.Output = Default_Intermediate_Output
	settings.link.LibMangle = function(name) error("no LibMangle function set") end
	settings.link.extension = ""
	settings.link.exe = ""
	settings.link.inputflags = ""
	settings.link.flags = NewFlagTable()
	settings.link.libs = NewTable()
	settings.link.frameworks = NewTable()
	settings.link.frameworkpath = NewTable()
	settings.link.libpath = NewTable()
	settings.link.extrafiles = NewTable()
	
	TableLock(settings.link)
end)

--[[@FUNCTION
	TODO
@END]]--
function Link(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = TableFlatten({...})

	output = settings.link.Output(settings, output) .. settings.link.extension
	settings.link.Driver(settings.labelprefix .. "link " .. output, output, inputs, settings)

	-- all the files
	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end
	
	for index, inname in ipairs(settings.link.extrafiles) do
		AddDependency(output, inname)
	end
	
	-- add the libaries
	local libs = {}
	local paths = {}
	
	for index, inname in ipairs(settings.link.libs) do
		table.insert(libs, settings.lib.prefix .. inname .. settings.lib.extension)
	end

	for index, inname in ipairs(settings.link.libpath) do
		table.insert(paths, inname)
	end
	
	AddDependencySearch(output, paths, libs)

	return output
end

------------------------ SHARED LIBRARY ACTION ------------------------

--[[@GROUP SharedLibrary @END]]--

AddTool("dll", function (settings)
	settings.dll = {}
	settings.dll.Driver = DriverNull
	settings.dll.prefix = ""
	settings.dll.extension = ""
	settings.dll.Output = Default_Intermediate_Output
	settings.dll.exe = ""
	settings.dll.inputflags = ""
	settings.dll.flags = NewFlagTable()
	settings.dll.libs = NewTable()
	settings.dll.frameworks = NewTable()
	settings.dll.frameworkpath = NewTable()
	settings.dll.libpath = NewTable()
	settings.dll.extrafiles = NewTable()

	TableLock(settings.dll)
end)

--[[@FUNCTION
	TODO
@END]]--
function SharedLibrary(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = TableFlatten({...})

	output = settings.dll.Output(settings, PathJoin(PathPath(output), settings.dll.prefix .. PathFilename(output))) .. settings.dll.extension
	settings.dll.Driver(settings.labelprefix .. "dll ".. output, output, inputs, settings)

	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end

	-- add the libaries
	local libs = {}
	local paths = {}
	
	for index, inname in ipairs(settings.dll.libs) do
		table.insert(libs, settings.dll.prefix .. inname .. settings.lib.extension)
	end

	for index, inname in ipairs(settings.dll.libpath) do
		table.insert(paths, inname)
	end
	
	AddDependencySearch(output, paths, libs)
	
	return output
end


------------------------ STATIC LIBRARY ACTION ------------------------

--[[@GROUP StaticLibrary @END]]--

AddTool("lib", function (settings)
	settings.lib = {}
	settings.lib.Driver = DriverNull
	settings.lib.Output = Default_Intermediate_Output
	settings.lib.prefix = ""
	settings.lib.extension = ""
	settings.lib.exe = ""
	settings.lib.flags = NewFlagTable()
	
	TableLock(settings.lib)
end)

--[[@FUNCTION
	TODO
@END]]--
function StaticLibrary(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = TableFlatten({...})

	output = settings.lib.Output(settings, PathJoin(PathPath(output), settings.lib.prefix .. PathFilename(output))) .. settings.lib.extension

	AddJob(output, settings.labelprefix .. "lib " .. output, settings.lib.Driver(output, inputs, settings))

	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end

	return output
end
