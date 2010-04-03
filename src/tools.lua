function SetDefaultDrivers(settings)
	-- set default drivers
	if family == "windows" then
		SetDriversCL(settings)
	else
		SetDriversGCC(settings)
	end
end

--[[@GROUP Common Settings (settings) @END]]--
function SetCommonSettings(settings)
	settings.Copy = TableDeepCopy
	
	--[[@FUNCTION name
		Name of the settings.
		TODO: explain when you could use it
	@END]]
	settings.name = ""

	--[[@FUNCTION config_ext
		A short postfix that you can append to files that have been built
		by this configuration.
	@END]]
	settings.config_ext = ""

	--[[@FUNCTION labelprefix
		Prefix to use for all jobs that are added.
		TODO: this option feels a bit strange
	@END]]
	settings.labelprefix = ""

	-- TODO: what todo with these
	settings.debug = 1
	settings.optimize = 0
end

------------------------ COMPILE ACTION ------------------------
	
--[[@GROUP Compile @END]]--

-- Compiles C, Obj-C and C++ files
--[[@UNITTESTS
	err=1; find="expected a settings object": Compile(nil)
	err=1; find="compiler returned a nil": s = NewSettings(); s.filemappings["c"] = function()end; Compile(s, "test.c")
	err=0 : s = NewSettings(); Compile(s)
@END]]--
--[[@FUNCTION Compile(settings, ...)
	Compiles a set of files using the supplied settings. It uses
	^settings.compile.filemappings^ to map the input extension to a compiler
	function. The function takes 2 parameters, the first being the settings
	table and the other one input filename. It adds the nessesary jobs and
	dependencies and returns the name of the generated object file.
@END]]--
function Compile(settings, ...)
	CheckSettings(settings)
	local outputs = {}
	local mappings = settings.compile.filemappings
	
	settings.invoke_count = settings.invoke_count + 1

	for inname in TableWalk({...}) do
		-- fetch correct compiler
		local ext = PathFileExt(inname)
		local Compiler = mappings[ext]

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

AddTool(function (settings)
	--[[@FUNCTION filemappings
		Table that matches extentions to a compiler function.
	@END]]
	settings.compile = {}
	settings.compile.filemappings = {}
end)

------------------------ C/C++ COMPILE ------------------------

--[[@GROUP C/C++ Compiler Settings (settings.cc) @END]]--

function DriverNull()
	error("no driver set")
end

function InitCommonCCompiler(settings)
	if settings.cc then return end
	
	settings.cc = {}
	settings.cc._c_cache = { nr = 0, str = "" }
	settings.cc._cxx_cache = { nr = 0, str = "" }

	--[[@FUNCTION extension
		Extention that the object files should have.
	@END]]
	settings.cc.extension = ""

	--[[@FUNCTION c_exe
		Name (and path) of the executable that is the C compiler
	@END]]
	settings.cc.c_exe = ""

	--[[@FUNCTION cxx_exe
		Same as c_exe but for the C++ compiler
	@END]]
	settings.cc.cxx_exe = ""

	--[[@FUNCTION DriverCTest
	@END]]
	settings.cc.DriverCTest = DriverNull
	
	--[[@FUNCTION DriverC
		Function that drives the C compiler. Function is responsible
		for building the command line and adding the job to compile the
		input file.
	@END]]
	settings.cc.DriverC = DriverNull
	
	--[[@FUNCTION DriverCXX
		Same as DriverC but for the C++ compiler.
	@END]]
	settings.cc.DriverCXX = DriverNull
	
	--[[@FUNCTION flags
		Table of flags that will be appended to the command line to the
		C/C++ compiler. These flags are used for both the C and C++
		compiler.
		
		{{{{
			settings.cc.flags:Add("-O2", "-g")
		}}}}
	@END]]
	settings.cc.flags = NewFlagTable()
	
	--[[@FUNCTION c_flags
		Same as flags but specific for the C compiler.
	@END]]
	settings.cc.c_flags = NewFlagTable()

	--[[@FUNCTION cxx_flags
		Same as flags but specific for the C compiler.
	@END]]
	settings.cc.cxx_flags = NewFlagTable()
	
	--[[@FUNCTION includes
		Table of paths where to find headers.
	@END]]
	settings.cc.includes = NewTable()
	
	--[[@FUNCTION systemincludes
		Table of paths where to find system headers.
	@END]]
	settings.cc.systemincludes = NewTable()
	
	--[[@FUNCTION defines
		Table of defines that should be set when compiling.
		
		{{{{
			settings.cc.defines:Add("ANSWER=42")
		}}}}
	@END]]
	settings.cc.defines = NewTable()

	--[[@FUNCTION frameworks
		Mac OS X specific. What frameworks to use when compiling.
	@END]]
	settings.cc.frameworks = NewTable()
	
	--[[@FUNCTION Output(path)
		Function that should transform the input path
		into the output path. The appending of the extension is done
		automaticly.
	@END]]
	settings.cc.Output = Default_Intermediate_Output

	TableLock(settings.cc)
end

function CompileC(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	settings.cc.DriverC(settings.labelprefix .. "c " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_dependency_cpp(input, settings.cc.includes)
	return outname
end

function CompileCXX(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	settings.cc.DriverCXX(settings.labelprefix .. "c++ " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_dependency_cpp(input, settings.cc.includes)	
	return outname
end


AddTool(function (settings)
	InitCommonCCompiler(settings)
	settings.compile.filemappings["c"] = CompileC
	settings.compile.filemappings["m"] = CompileC
end)

AddTool(function (settings)
	InitCommonCCompiler(settings)
	settings.compile.filemappings["cpp"] = CompileCXX
	settings.compile.filemappings["cxx"] = CompileCXX
	settings.compile.filemappings["c++"] = CompileCXX
	settings.compile.filemappings["cc"] = CompileCXX
end)

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

AddTool(function (settings)
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

AddTool(function (settings)
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

AddTool(function (settings)
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
