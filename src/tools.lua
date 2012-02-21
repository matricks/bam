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
	
	--[[@FUNCTION config_name
		Name of the settings.
		TODO: explain when you could use it
	@END]]
	settings.config_name = ""

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
	err=1; find="compiler returned a nil": s = NewSettings(); s.mappings["c"] = function()end; Compile(s, "test.c")
	err=0 : s = NewSettings(); Compile(s)
@END]]--
--[[@FUNCTION Compile(settings, ...)
	Compiles a set of files using the supplied settings. It uses
	^settings.compile.mappings^ to map the input extension to a compiler
	function. A compiler functions should look like ^Compiler(settings, input)^
	where ^settings^ is the settings object and ^input^ is the filename
	of the file to compile. The function should return a string that
	contains the object file that it will generate.

	{{{{
	function MyCompiler(settings, input)
	\t-- compile stuff
	\treturn output
	end
	
	settings.compile.mappings[".my"] = MyCompiler
	objects = Compile(settings, "code.my") -- Invokes the MyCompiler function
	}}}}
	
@END]]--
function Compile(settings, ...)
	CheckSettings(settings)
	local outputs = {}
	local mappings = settings.compile.mappings
	local insert = table.insert
	
	-- TODO: this here is aware of the different compilers, should be moved somewhere
	bam_add_dependency_cpp_set_paths(settings.cc.includes)
	
	settings.invoke_count = settings.invoke_count + 1
	for inname in TableWalk({...}) do
		-- fetch correct compiler
		local ext = PathFileExt(inname)
		local Compiler = mappings[ext]

		if not Compiler then
			error("'"..inname.."' has unknown extention '"..ext.."' which there are no compiler for")
		end
		
		insert(outputs, Compiler(settings, inname))
	end
	
	-- return the output
	return outputs
end

function CTestCompile(settings, code, options)
	return settings.cc.DriverCTest(code, options)
end

AddTool(function (settings)
	--[[@FUNCTION Compile Settings (settings.compile)
		<table>
		<tr><td>^mappings^</td><td>
		Table that matches extentions to a compiler function. See the
		Compile function for a reference how this table is used.
		</td></tr>
		</table>
	@END]]
	settings.compile = {}
	settings.compile.mappings = {}
	TableLock(settings.compile)
end)

------------------------ C/C++ COMPILE ------------------------

--[[@FUNCTION C/C++ Settings (settings.cc)
<table>
@PAUSE]]

function DriverNull()
	error("no driver set")
end

function InitCommonCCompiler(settings)
	if settings.cc then return end
	
	settings.cc = {}
	settings.cc._c_cache = { nr = 0, str = "" }
	settings.cc._cxx_cache = { nr = 0, str = "" }

	--[[@RESUME
		<tr><td>^defines^</td><td>
		Table of defines that should be set when compiling.
		
		{{{{
			settings.cc.defines:Add("ANSWER=42")
		}}}}
		</td></tr>
	@PAUSE]]
	settings.cc.defines = NewTable()

	--[[@RESUME
	@PAUSE]]
	settings.cc.DriverCTest = DriverNull
	
	--[[@RESUME
		<tr><td>^DriverC^</td><td>
		Function that drives the C compiler. Function is responsible
		for building the command line and adding the job to compile the
		input file.
		</td></tr>
	@PAUSE]]
	settings.cc.DriverC = DriverNull
	
	--[[@RESUME
		<tr><td>^DriverCXX^</td><td>
		Same as DriverC but for the C++ compiler.
		</td></tr>
	@PAUSE]]
	settings.cc.DriverCXX = DriverNull

	--[[@RESUME
		<tr><td>^exe_c^</td><td>Name (and path) of the executable that is the C compiler</td></tr>
	@PAUSE]]
	settings.cc.exe_c = ""

	--[[@RESUME
		<tr><td>^exe_cxx^</td><td>Same as c_exe but for the C++ compiler</td></tr>
	@PAUSE]]
	settings.cc.exe_cxx = ""

	--[[@RESUME
		<tr><td>^extension^</td><td>
		Extention that the object files should have. Usally ".o" or ".obj"
		depending on compiler tool chain.
		</td></tr>
	@PAUSE]]
	settings.cc.extension = ""

		
	--[[@RESUME
		<tr><td>^flags^</td><td>
		Table of flags that will be appended to the command line to the
		C/C++ compiler. These flags are used for both the C and C++
		compiler.
		
		{{{{
			settings.cc.flags:Add("-O2", "-g")
		}}}}
		</td></tr>
	@PAUSE]]
	settings.cc.flags = NewFlagTable()
	
	--[[@RESUME
		<tr><td>^flags_c^</td><td>
		Same as flags but specific for the C compiler.
		</td></tr>
	@PAUSE]]
	settings.cc.flags_c = NewFlagTable()

	--[[@RESUME
		<tr><td>^flags_cxx^</td><td>
		Same as flags but specific for the C++ compiler.
		</td></tr>
	@PAUSE]]
	settings.cc.flags_cxx = NewFlagTable()

	--[[@RESUME
		<tr><td>^frameworks^</td><td>
		Mac OS X specific. What frameworks to use when compiling.
		</td></tr>
	@PAUSE]]
	settings.cc.frameworks = NewTable()
			
	--[[@RESUME
		<tr><td>^includes^</td><td>
		Table of paths where to find headers.
		
		{{{{
			settings.cc.includes:Add("my/include/directory")
		}}}}		
		</td></tr>
	@PAUSE]]
	settings.cc.includes = NewTable()
	
	--[[@RESUME
		<tr><td>^Output(settings, path)^</td><td>
		Function that should transform the input path
		into the output path. The appending of the extension is done
		automaticly.
		
		{{{{
			settings.cc.Output = function(settings, input)
			&nbsp;&nbsp;&nbsp;&nbsp;return PathBase(input) .. settings.config_ext
			end
		}}}}
		</td></tr>
	@PAUSE]]
	settings.cc.Output = Default_Intermediate_Output
	
	--[[@RESUME
		<tr><td>^systemincludes^</td><td>
		Mac OS X specific. Table of paths where to find system headers.
		</td></tr>
	@PAUSE]]
	settings.cc.systemincludes = NewTable()

	--[[@RESUME
		</table>
	@END]]
	
	TableLock(settings.cc)
end

function CompileC(settings, input)
	local cc = settings.cc
	local outname = cc.Output(settings, input) .. cc.extension
	cc.DriverC(settings.labelprefix .. "c " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_add_dependency_cpp(input)
	return outname
end

function CompileCXX(settings, input)
	local cc = settings.cc
	local outname = cc.Output(settings, input) .. cc.extension
	cc.DriverCXX(settings.labelprefix .. "c++ " .. input, outname, input, settings)
	AddDependency(outname, input)
	bam_add_dependency_cpp(input)
	return outname
end


AddTool(function (settings)
	InitCommonCCompiler(settings)
	settings.compile.mappings["c"] = CompileC
	settings.compile.mappings["m"] = CompileC
	settings.compile.mappings["S"] = CompileC
end)

AddTool(function (settings)
	InitCommonCCompiler(settings)
	settings.compile.mappings["cpp"] = CompileCXX
	settings.compile.mappings["cxx"] = CompileCXX
	settings.compile.mappings["c++"] = CompileCXX
	settings.compile.mappings["cc"] = CompileCXX
end)

--[[@GROUP Other @END]]--

--[[@FUNCTION CopyFile(dst, src)
@END]]--
if family == "windows" then
	function CopyFile(dst, src)
		AddJob(dst,
			"copy " .. src .. " -> " .. dst,
			"copy /b \"" .. str_replace(src, "/", "\\") .. "\" \"" .. str_replace(dst, "/", "\\") .. "\" >nul 2>&1",
			src)
		return dst
	end
else
	function CopyFile(dst, src)
		AddJob(dst,
			"copy " .. src .. " -> " .. dst,
			"cp " .. src .. " " .. dst,
			src)
		return dst
	end
end

--[[@FUNCTION CopyToDirectory(dst, ...)
@END]]--
function CopyToDirectory(dst, ...)
	local insert = table.insert
	local outputs = {}
	for src in TableWalk({...}) do
		insert(outputs, CopyFile(PathJoin(dst, PathFilename(src)), src))
	end
	return outputs
end

------------------------ LINK ------------------------

--[[@GROUP Link @END]]--

--[[@FUNCTION
	TODO
@END]]--
function Link(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = TableFlatten({...})

	output = settings.link.Output(settings, output) .. settings.link.extension
	settings.link.Driver(settings.labelprefix .. "link " .. output, output, inputs, settings)

	-- all the files
	AddDependency(output, inputs)
	AddDependency(output, settings.link.extrafiles)
	
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


--[[@FUNCTION Settings (settings.link)
<table>
@PAUSE]]

AddTool(function (settings)
	settings.link = {}
	
	--[[@RESUME
		<tr><td>^Driver^</td><td>
		Function that drives the linker. Function is responsible
		for building the command line and adding the job to link the
		input files into an executable.
		</td></tr>
	@PAUSE]]	
	settings.link.Driver = DriverNull

	--[[@RESUME
		<tr><td>^exe^</td><td>
		Path to the executable to use as linker.
		</td></tr>
	@PAUSE]]	
	settings.link.exe = ""
	
	--[[@RESUME
		<tr><td>^extension^</td><td>
		Extention of the executable. Usally "" on most platform but can
		be ".exe" on platforms like Windows.
		</td></tr>
	@PAUSE]]	
	settings.link.extension = ""

	--[[@RESUME
		<tr><td>^extrafiles^</td><td>
		A table of additional files that should be linked against. These
		files will be treated as normal objects when linking.
		</td></tr>
	@PAUSE]]	
	settings.link.extrafiles = NewTable()

	--[[@RESUME
		<tr><td>^flags^</td><td>
		Table of raw flags to send to the linker.
		{{{{
			settings.link.flags:Add("-v")
		}}}}
		</td></tr>
	@PAUSE]]	
	settings.link.flags = NewFlagTable()

	--[[@RESUME
		<tr><td>^frameworks^</td><td>
		Mac OS X specific. A table of frameworks to link against.
		</td></tr>
	@PAUSE]]	
	settings.link.frameworks = NewTable()

	--[[@RESUME
		<tr><td>^frameworkpath^</td><td>
		Mac OS X specific. A table of paths were to find frameworks.
		</td></tr>
	@PAUSE]]	
	settings.link.frameworkpath = NewTable()

	--[[@RESUME
		<tr><td>^libs^</td><td>
		Table of library files to link with.
		{{{{
			settings.link.libs:Add("pthread")
		}}}}
		</td></tr>
	@PAUSE]]	
	settings.link.libs = NewTable()

	--[[@RESUME
		<tr><td>^libpath^</td><td>
		A table of paths of where to find library files that could be
		included in the linking process.
		</td></tr>
	@PAUSE]]	
	settings.link.libpath = NewTable()
	
	--[[@RESUME
		<tr><td>^Output^</td><td>
		Function that should transform the input path
		into the output path. The appending of the extension is done
		automaticly.
		
		{{{{
			settings.link.Output = function(settings, input)
			&nbsp;&nbsp;&nbsp;&nbsp;return PathBase(input) .. settings.config_ext
			end
		}}}}
		</td></tr>
	@PAUSE]]	
	settings.link.Output = Default_Intermediate_Output
	
	--[[@RESUME
		<tr><td>inputflags (REMOVE?)</td><td>
		</td></tr>
	@PAUSE]]	
	settings.link.inputflags = ""

	--[[@RESUME
		</table>
	@END]]
		
	TableLock(settings.link)
end)

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

--[[@<FUNCTION
	TODO
@END]]--
function SharedLibrary(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = TableFlatten({...})

	output = settings.dll.Output(settings, PathJoin(PathDir(output), settings.dll.prefix .. PathFilename(output))) .. settings.dll.extension
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

	output = settings.lib.Output(settings, PathJoin(PathDir(output), settings.lib.prefix .. PathFilename(output))) .. settings.lib.extension

	AddJob(output, settings.labelprefix .. "lib " .. output, settings.lib.Driver(output, inputs, settings))

	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end

	return output
end
