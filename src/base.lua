-- just install a better name for the functions and tables
ScriptArgs = _bam_scriptargs
IsString = bam_isstring
IsTable = bam_istable
Exist = bam_fileexist
NodeExist = bam_nodeexist
SetFilter = bam_set_filter
AddOutput = bam_add_output

--[[@GROUP Common @END]]--

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

--[[@FUNCTION Execute(command)
	Executes the ^command^ in the shell and returns the error code.
@END]]--
Execute = os.execute

--[[@FUNCTION ExecuteSilent(command)
	Does the same as ^Execute(command)^ but supresses stdout and stderr of
	that command.
@END]]--
if family == "windows" then
	ExecuteSilent = function(command) return os.execute(command .. " >nul 2>&1") end
else
	ExecuteSilent = function(command) return os.execute(command .. " >/dev/null 2>/dev/null") end
end

--[[@GROUP Path Manipulation @END]]--

--[[@UNITTESTS
	err=1 : Path(nil)
	err=1 : Path({})
	err=1 : Path("asdf", "asdf")
	catch="" : Path("")
	catch="" : Path("/")
	catch="/b.c/file.ext" : Path("/a/../b.c/./file.ext")
	catch="/b.c" : Path("/a/../b.c/./")
	catch="/.bc" : Path("/a/../.bc/./")
	catch="/a" : Path("/a/b/..")
	catch="/a/..b" : Path("/a/..b/")
	catch="/a/b" : Path("/a//b/")
	catch="/a" : Path("/a/b/../")
	catch="/a" : Path("/a/.b/../")
	catch="/a/.b" : Path("/a/.b")
	catch="/a/.b" : Path("/a/.b/")
	catch="../../b.c" : Path("../../a/../b.c/./")
	catch="../path/file.name.ext" : Path("../test/../path/file.name.ext")
@END]]--
--[[@FUNCTION Path(str)
	Normalizes the path ^str^ by removing ".." and "." from it.

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
	PathBase("test/path/file.name.ext") -- Returns "test/path/file.name"
	PathBase("test/path/file.name") -- Returns "test/path/file"
	PathBase("test/path/file") -- Returns "test/path/file"
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

--[[@UNITTESTS
	err=1 : PathJoin(nil)
	err=1 : PathJoin("asdf", "asdf", "asdf")
	catch="a/b" : PathJoin("a/b", "")
	catch="a/b" : PathJoin("a/b/", "")
	catch="a/b" : PathJoin("a", "b")
	catch="a" : PathJoin("", "a")
	catch="a/b" : PathJoin("", "a/b")
	catch="a" : PathJoin("a/b", "..")
	catch="a/b" : PathJoin("a/b/", "")
	catch="a/b" : PathJoin("a/", "b/")
@END]]--
--[[@FUNCTION PathJoin(base, add)
	Joins the two paths ^base^ and ^add^ together and returns a
	normalized path. This function haldes trailing path separators in
	the ^base^ argument.
		
	{{{{
	PathJoin("test/path/", "../filename.ext") -- Returns "test/filename.ext"
	PathJoin("../test", "path/filename.ext") -- Returns "../test/path/filename.ext"
	}}}}	
	
@END]]--
PathJoin = bam_path_join

--[[@UNITTESTS
	err=1 : PathDir(nil)
	err=1 : PathDir({})
	catch="" : PathDir("")
	catch="" : PathDir("/")
	catch="/b.c" : PathDir("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION PathDir(str)
	Returns the path of the filename in ^str^.

	{{{{
	PathDir("test/path/file.name.ext") -- Returns "test/path"
	}}}}
@END]]--
PathDir = bam_path_dir

--[[@GROUP Tables @END]]--

--[[@UNITTESTS
	err=1 : TableDeepCopy(nil)
	err=1 : TableDeepCopy("")
	err=1 : TableDeepCopy({}, {})
	err=0 : TableDeepCopy({{{"a"}, "b"}, "c", "d", {"e", "f"}})
@END]]--	
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
	Does a deep walk of the ^tbl^ table for strings and generates a new
	flat table with the strings. If it occurs anything else then a table
	or string, it will generate an error.
	
	{{{{
	-- Returns {"a", "b", "c", "d", "e", "f"}
	TableFlatten({"a", {"b", {"c"}, "d"}, "e", {}, {"f"}})
	}}}}
@END]]--
TableFlatten = bam_table_flatten

--[[@UNITTESTS
	err=0 : t = {a = 1}; TableLock(t); t.a = 2
	err=1 : t = {a = 1}; TableLock(t); t.b = 2
@END]]--
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
	catch="[a][b]" : TableToString({"a", "b"}, "[", "]")
@END]]--
--[[@FUNCTION TableToString(tbl, prefix, postfix)
	Takes every string element in the ^tbl^ table, prepends ^prefix^ and appends ^postfix^
	to each element and returns the result.
	
	{{{{
	TableToString({"a", "b"}, "[", "]") -- Returns "[a][b]"
	}}}}
@END]]--
TableToString = bam_table_tostring

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
	when ^NewSettings^ function is invoked with the settings object as
	first parameter.
@END]]--
function AddTool(func)
	table.insert(_bam_tools, func)
end

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

--[[@UNITTESTS
	err=0 : NewSettings()
	err=1 : t = NewSettings(); t.cc.DOES_NOT_EXIST = 1
	err=1 : t = NewSettings():Copy(); t.cc.DOES_NOT_EXIST = 1
@END]]--
--[[@FUNCTION
	Create a new settings table with the settings for all the registered
	tools. This table is passed to many of the tools and contains how
	they should act.
@END]]--
function NewSettings()
	local settings = {}
	
	settings._is_settingsobject = true
	settings.invoke_count = 0
	
	SetCommonSettings(settings)
	
	-- add all tools
	for _, tool in pairs(_bam_tools) do
		tool(settings)
	end
	
	-- HACK: setup default drivers
	SetDefaultDrivers(settings)

	-- lock the table and return
	TableLock(settings)	
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

--[[@FUNCTION MakeDirectory(path)
	Creates the requested directory.
@END]]--
MakeDirectory = bam_mkdir

--[[@FUNCTION MakeDirectories(filename)
	Creates the path upto the filename.

	Example:
	{{{{
		MakeDirectories("output/directory/object.o")
	}}}}

	This will create the complete "output/directory" path.
@END]]--
MakeDirectories = bam_mkdirs

--[[@GROUP Targets@END]]--

--[[@FUNCTION DefaultTarget(filename)
	Specifies the default target use build when no targets are
	specified when bam is invoked.
@END]]--
DefaultTarget = bam_default_target

--[[@FUNCTION
	Creates a pseudo target named ^name^ and assigns a set of dependencies
	specified by ^...^.
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

--[[@FUNCTION Import(filename)
	Imports a script specified by ^filename^. A search for the script will be
	done by first checking the current directory and then the paths specified
	by the BAM_PACKAGES environment variable. Several paths can be specified
	in the variable by separating them by a ':' character.

	The importing script can figure out it's path by calling the
	[ModuleFilename] function.
@END]]--
function Import(filename)
	local paths = {"", PathDir(ModuleFilename())}

	s = os.getenv("BAM_PACKAGES")
	if s then
		for w in string.gmatch(s, "[^:]*") do
			if string.len(w) > 0 then
				table.insert(paths, w)
			end
		end
	end
	
	for _,path in pairs(paths) do
		local filepath = PathJoin(path, filename)
		if Exist(filepath) then
			local chunk = bam_loadfile(filepath)
			if chunk then
				local current = _bam_modulefilename
				_bam_modulefilename = filepath
				bam_update_globalstamp(_bam_modulefilename)
				chunk()
				_bam_modulefilename = current
				return
			end
		end
	end

	error(filename .. " not found")
end


--[[@FUNCTION
	Returns the filename of the current script being imported (by [Import])
	as relative to the current working directory.
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
		AddJob("myapp", "linking myapp", "gcc myapp1.o myapp2.o myapp3.o -o myapp.o", {"myapp1.o", "myapp2.o"}, "myapp3.o")
	}}}}
@END]]--
AddJob = bam_add_job

--[[@FUNCTION AddDependency(filename, ...)
	Adds dependencies to a job. The files specified in the argument list gets added.
	Strings and nested tables of strings are accepted.
@END]]--
AddDependency = bam_add_dependency

--[[@FUNCTION AddDependencySearch(filename, paths, ...)
	Searches for dependencies in the specified ^paths^ and adds them to
	the ^file^.
@END]]--
AddDependencySearch = bam_add_dependency_search

function Default_Intermediate_Output(settings, input)
	return PathBase(input) .. settings.config_ext
end

-- [TODO: Should be in C?]
function str_replace(s, pattern, what)
	return string.gsub(s, pattern, function(v) return what end)
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
	
	t.Remove = function(self, val)
		local tmp = {}
		for k,v in ipairs(self) do
			if v == val then
				table.remove(self, k)
			end
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
		
		local s = TableToString(self, nil, " ")
		
		self.string = s
		self.string_version = self.version
		
		return s
	end

	t.string_version = 0
	t.string = ""

	return t
end

AddConstraintShared = bam_add_constraint_shared
AddConstraintExclusive = bam_add_constraint_exclusive

