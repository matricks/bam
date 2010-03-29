-- just install a better name for the functions and tables
ScriptArgs = _bam_scriptargs
Execute = os.execute
IsString = bam_isstring
IsTable = bam_istable
MakeDirectory = bam_mkdir
Exist = bam_fileexist
SetFilter = bam_set_filter
SetTouch = bam_set_touch
AddOutput = bam_add_output

--[[@GROUP Common @END]]--

--[[@UNITTESTS
	err=1; find="expected a settings object": CheckSettings(nil)
	err=1; find="expected a settings object": CheckSettings("")
	err=1; find="expected a settings object": CheckSettings({})
	err=0 : CheckSettings(NewSettings())
@END]]--
function CheckSettings(settings)
	if not IsTable(settings) or settings._is_settingsobject == nil then
		error("expected a settings object, got an " .. type(settings) .. " instead")
	end
end

--[[@FUNCTION CheckVersion
	Tells bam what version this script is written for. It will either
	make sure that it behaves like that version or print out an error.
	
	{{{{
	CheckVersion("0.1.0")
	}}}}
@END]]--
function CheckVersion(version)
	if version == _bam_version then
	else
		error("this file for version "..version..".x of bam. you are running ".._bam_version..".x.")
	end
end

--[[@GROUP Path Manipulation @END]]--

--[[@UNITTESTS
	err=1 : Path(nil)
	err=1 : Path({})
	catch="" : Path("")
	catch="" : Path("/")
	catch="/b.c/file.ext" : Path("/a/../b.c/./file.ext")
	catch="/b.c" : Path("/a/../b.c/./")
	catch="/a" : Path("/a/b/..")
	catch="../../b.c" : Path("../../a/../b.c/./")
	catch="../path/file.name.ext" : Path("../test/../path/file.name.ext")
@END]]--
--[[@FUNCTION Path(str)
	Normalizes the path in ^str^ by removing ".." and "." from it.

	{{{{
	Path("test/./path/../file.name.ext") -- Returns "test/file.name.ext"
	Path("../test/../path/file.name.ext") -- Returns "../path/file.name.ext"
	}}}}
@END]]--
Path = bam_path_normalize

--[[@UNITTESTS
	err=1: PathBase(nil)
	err=1: PathBase({})
	catch="": PathBase("")
	catch="/": PathBase("/")
	catch="test/path/file.name": PathBase("test/path/file.name.ext")
	catch="/a/../b.c/./file": PathBase("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION PathBase(path)
	Returns the everthing except the extention in the path.

	{{{{
	Path("test/path/file.name.ext") -- Returns "test/path/file.name"
	Path("test/path/file.name") -- Returns "test/path/file"
	Path("test/path/file") -- Returns "test/path/file"
	}}}}
@END]]--
PathBase = bam_path_base

--[[@UNITTESTS
	err=1 : PathFileExt(nil)
	err=1 : PathFileExt({})
	catch="" : PathFileExt("")
	catch="" : PathFileExt("/")
	catch="ext" : PathFileExt("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION PathFileExt(str)
	Returns the extension of the filename in ^str^.
	
	{{{{
	PathFileExt("test/path/file.name.ext") -- Returns "ext"
	}}}}
@END]]--
PathFileExt = bam_path_ext

--[[@UNITTESTS
	err=1 : PathFilename(nil)
	err=1 : PathFilename({})
	catch="" : PathFilename("")
	catch="" : PathFilename("/")
	catch="file.ext" : PathFilename("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION PathFilename(str)
	Returns the filename of the path in ^str^.

	{{{{
	PathFilename("test/path/file.name.ext") -- Returns "file.name.ext"
	}}}}
@END]]--
PathFilename = bam_path_filename

--[TODO: Should be in C]
--[[@UNITTESTS
	err=1 : PathJoin(nil)
	catch="a/b" : PathJoin("a/b", "")
	catch="a/b" : PathJoin("a/b/", "")
	catch="a/b" : PathJoin("a", "b")
	catch="a" : PathJoin("", "a")
	catch="a/b" : PathJoin("", "a/b")
	catch="a" : PathJoin("a/b", "..")
@END]]--
--[[@FUNCTION PathJoin(base, add)
	Joins the two paths ^base^ and ^add^ together and returns a
	normalized path.
		
	{{{{
	PathJoin("test/path/", "../filename.ext") -- Returns "test/filename.ext"
	PathJoin("../test", "path/filename.ext") -- Returns "../test/path/filename.ext"
	}}}}	
	
@END]]--
function PathJoin(base, add)
	if string.len(base) == 0 then
		return Path(add)
	end
	
	if string.sub(base, -1) == "/" then
		if string.len(add) == 0 then
			return string.sub(base, 0, string.len(base)-1)
		end
		return Path(base .. add)
	end
	
	if string.len(add) == 0 then
		return Path(base)
	end
	
	return Path(base .. "/" .. add)
end

--[[@UNITTESTS
	err=1 : PathPath(nil)
	err=1 : PathPath({})
	catch="" : PathPath("")
	catch="" : PathPath("/")
	catch="/b.c" : PathPath("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION PathPath(str)
	Returns the path of the filename in ^str^.

	{{{{
	PathPath("test/path/file.name.ext") -- Returns "test/path"
	}}}}
@END]]--
PathPath = bam_path_path

-- [TODO: Should be in C?]
function str_replace(s, pattern, what)
	return string.gsub(s, pattern, function(v) return what end)
end

-- make a table into a string
-- [TODO: Should be in C?]
-- [TODO: Should be removed?]
function tbl_to_str(tbl, prefix, postfix)
	local s = ""
	for index,value in ipairs(tbl) do
		if IsString(value) then
			s = s .. prefix .. value .. postfix
		end
	end
	return s
end

function NewTable()
	local t = {}

	t.Add = function(self, ...)
		for i,what in ipairs({...}) do
			table.insert(self, what)
		end
		self.version = self.version + 1
	end

	t.Merge = function(self, source)
		for k,v in ipairs(source) do
			table.insert(self, v)
		end
		self.version = self.version + 1
	end

	t.version = 0

	return t
end

function NewFlagTable()
	local t = NewTable()

	t.ToString = function(self)
		if self.string_version == self.version then
			return self.string
		end
		
		local s = ""
		for key,value in pairs(self) do
			if type(value) == type("") then
				s = s .. value .. " "
			end
		end
		
		self.string = s
		self.string_version = self.version
		
		return s
	end

	t.string_version = 0
	t.string = ""

	return t
end


--[[@GROUP Tables @END]]--

--[[@FUNCTION TableDeepCopy(tbl)
	Makes a deep copy of the table ^tbl^ resulting in a complete separate
	table.
@END]]--
TableDeepCopy = bam_table_deepcopy

--[[@UNITTESTS
	err=0 : TableFlatten({"", {"", {""}, ""}, "", {}, {""}})
	err=1 : TableFlatten({"", {"", {""}, ""}, 1, {""}})
@END]]--
--[[@FUNCTION TableFlatten(tbl)
	Flattens a tree of tables
@END]]--
function TableFlatten(varargtable)
	function flatten(collection, varargtable)
		for i,v in pairs(varargtable) do
			if IsTable(v) then
				flatten(collection, v)
			elseif IsString(v) then
				table.insert(collection, v)
			else
				error("unexpected " .. type(v) .. " in table")
			end		
		end
	end

	local inputs = {}
	flatten(inputs, varargtable)
	return inputs
end

--[[@FUNCTION TableLock(tbl)
	Locks the table ^tbl^ so no new keys can be added. Trying to add a new
	key will result in an error.
@END]]--
function TableLock(tbl)
	local mt = getmetatable(tbl)
	if not mt then mt = {} end
	mt.__newindex = function(tbl, key, value)
		error("trying to create key '" .. key .. "' on a locked table")
	end
	setmetatable(tbl, mt)
end

--[[@UNITTESTS
	err=0 : for s in TableWalk({"", {"", {""}, ""}, "", {}, {""}}) do end
	err=1 : for s in TableWalk({"", {"", {""}, ""}, 1, {""}}) do end
@END]]--
--[[@FUNCTION TableWalk(tbl)
	Returns an iterator that does a deep walk of a table looking for strings.
	Only checks numeric keys and anything else then table and strings will
	cause an error.

	{{{{
	for filename in TableWalk({...}) do
		print(filename)
	end
	}}}}
@END]]--
TableWalk = bam_table_walk

--[[@GROUP Settings @END]]--

_bam_tools = {}

--[[@UNITTESTS
@END]]--
--[[@FUNCTION
	Adds a new tool called ^name^ to bam. The ^func^ will be called
	when NewSettings function is invoked with the settings object as
	first parameter.
@END]]--
function AddTool(name, func)
	_bam_tools[name] = func
end

--[[@UNITTESTS
@END]]--
--[[@FUNCTION
	Create a new settings object with the settings for all the
	registered tools.
@END]]--
function NewSettings()
	local settings = {}
	
	settings._is_settingsobject = true
	settings.invoke_count = 0
	settings.Copy = TableDeepCopy
	
	settings.filemappings = {}

	settings.config_name = ""
	settings.config_ext = ""
	settings.labelprefix = ""
	
	settings.debug = 1
	settings.optimize = 0
	
	-- add all tools
	for _, tool in pairs(_bam_tools) do
		tool(settings)
	end

	TableLock(settings)
	
	-- set default drivers
	if family == "windows" then
		SetDriversCL(settings)
	else
		SetDriversGCC(settings)
	end
	
	return settings
end


--[[@GROUP Files and Directories @END]]--

-- Collects files in a directory.
--[[@FUNCTION Collect(...)
	Gathers a set of files using wildcard. Accepts strings and tables
	of strings as input and returns a table of all the files that
	matches A single wildcard * may be used in each string to collect
	a set of files.
	
	Example:
	{{{{
		source_files = Collect("src/*.c", "lib/*.c")
	}}}}
	
	Note. This version collects files, non-recursive.
@END]]--
Collect = bam_collect

--[[@FUNCTION CollectRecursive(...)
	Collects files as the [Collect] but does so recursivly.
@END]]--
CollectRecursive = bam_collectrecursive

--[[@FUNCTION CollectDirs(...)
	Collects directories in the same fashion as [Collect] but returns
	directories instead.
@END]]--
CollectDirs = bam_collectdirs

--[[@FUNCTION CollectDirsRecursive(...)
	Collects directories in the same fashion as [Collect] but does so
	recursivly and returns directories instead.
@END]]--
CollectDirsRecursive = bam_collectdirsrecursive

--[[@GROUP Targets@END]]--

--[[@FUNCTION DefaultTarget(filename)
	Specifies the default target use build when no targets are
	specified when bam is invoked.
@END]]--
DefaultTarget = bam_default_target

--[[@FUNCTION
	TODO	
@END]]--
function PseudoTarget(name, ...)
	local name = Path(name)
	bam_add_pseudo(name)
	
	-- all the files
	for inname in TableWalk({...}) do
		AddDependency(name, inname)
	end

	return name
end

--[[@GROUP Modules@END]]--

--[[@FUNCTION
	TODO
@END]]--
function Import(modname)
	local paths = {""}
	local chunk = nil

	s = os.getenv("BAM_PACKAGES")
	if s then
		for w in string.gmatch(s, "[^:]*") do
			if string.len(w) > 0 then
				table.insert(paths, w)
			end
		end
	end
	
	for k,v in pairs(paths) do
		chunk = bam_loadfile(modname)
		if chunk then
			local current = _bam_modulefilename
			_bam_modulefilename = modname
			bam_update_globalstamp(_bam_modulefilename)
			chunk()
			_bam_modulefilename = current
			return
		end
	end

	error(modname .. " not found")
end


--[[@FUNCTION
	TODO
@END]]--
function ModuleFilename()
	return _bam_modulefilename
end

--[[@GROUP Job and Dependencies @END]]--

--[[@FUNCTION AddJob(output, label, command, ...)
	Adds a job to be done. The ^output^ string specifies the file that
	will be created by the command line specified in ^command^ string.
	The ^label^ is printed out before ^command^ is runned. You can also
	add extra parameters, those will become for dependencies for the job.
	{{{{
		AddJob("myapp.o", "compiling myapp.c", "gcc -c myapp.c -o myapp.o")
		AddDependency("myapp.o", "myapp.c")
	}}}}
	This is the same as this:
	{{{{
		AddJob("myapp.o", "compiling myapp.c", "gcc -c myapp.c -o myapp.o", "myapp.c")
	}}}}
	You can also add several dependencies at once like this:
	{{{{
		AddJob("myapp", "linking myapp", "gcc myapp1.o myapp2.o -o myapp.o", {"myapp1.o", "myapp1.o"})
	}}}}
@END]]--
-- TODO: Implement this in C
function AddJob(output, label, command, ...)
	bam_add_job(output, label, command)
	AddDependency(output, {...})
end

--[[@FUNCTION AddDependency(filename, ...)
	Adds dependencies to a job. The files specified in the argument list gets added.
	Strings and nested tables of strings are accepted.
@END]]--
AddDependency = bam_add_dependency

--[[@FUNCTION AddDependencySearch(filename, paths, dependencies)
@END]]--
AddDependencySearch = bam_add_dependency_search
--[[
function AddDependencySearch(filename, paths, ...)
	bam_add_dependency_search(filename, paths, ...)
	AddDependency(output, {...})
end]]--


-- TODO: document
AddConstraintShared = bam_add_constraint_shared
AddConstraintExclusive = bam_add_constraint_exclusive

function Default_Intermediate_Output(settings, input)
	return PathBase(input) .. settings.config_ext
end

function DriverNull()
	error("no driver set")
end

--[[@GROUP Actions @END]]--

------------------------ C/C++ COMPILE ------------------------

function InitCommonCCompiler(settings)
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


--[[@GROUP Other @END]]--

--- SilentExecute
function _execute_silent_win(command) return os.execute(command .. " >nul 2>&1") end
function _execute_silent_unix(command) return os.execute(command .. " >/dev/null 2>/dev/null") end

--[[@FUNCTION ExecuteSilent(command)
	Executes a command in the shell and returns the error code. It supresses stdout and stderr
	of that command.
@END]]--
if family == "windows" then
	ExecuteSilent = _execute_silent_win
else
	ExecuteSilent = _execute_silent_unix
end
