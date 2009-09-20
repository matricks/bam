-- just install a better name for the functions and tables
ScriptArgs = _bam_scriptargs
Execute = os.execute
IsString = bam_isstring
IsTable = bam_istable

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

--[[@FUNCTION
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

--[[@FUNCTION
	Flattens a tree of tables
@END]]--
function FlattenTable(varargtable)
	function flatten(collection, varargtable)
		for i,v in pairs(varargtable) do
			if IsTable(v) then
				flatten(collection, v)
			elseif IsString(v) then
				table.insert(collection, v)
			end		
		end
	end

	local inputs = {}
	flatten(inputs, varargtable)
	return inputs
end

--[[@FUNCTION
	
@END]]--
function WalkTable(t)
  return coroutine.wrap(function()
    return WalkTableImpl(coroutine.yield, t)
  end)
end

function WalkTableImpl(func, varargtable)
	function walk(varargtable)
		for i,v in pairs(varargtable) do
			if IsTable(v) then
				walk(v)
			elseif IsString(v) then
				func(v)
			end		
		end
	end
	walk(varargtable)
end

--[[@GROUP Path Manipulation @END]]--

--[[@UNITTESTS
	err=1 : Path(nil)
	err=1 : Path({})
	catch="" : Path("")
	catch="" : Path("/")
	catch="/b.c/file.ext" : Path("/a/../b.c/./file.ext")
	catch="/b.c" : Path("/a/../b.c/./")
@END]]--
--[[@FUNCTION Path(str)
	Normalizes the path in ^str^ by removing ".." and "." from it

	{{{{
	Path("test/./path/../file.name.ext") -- Returns "test/file.name.ext"
	}}}}
@END]]--
Path = bam_path_fix

-- [TODO: Should be in C]
--[[@UNITTESTS
	err=1: PathBase(nil)
	err=1: PathBase({})
	catch="": PathBase("")
	catch="/": PathBase("/")
	catch="/a/../b.c/./file": PathBase("/a/../b.c/./file.ext")
@END]]--
--[[@FUNCTION
	Returns the everthing except the extention in the path.

	{{{{
	Path("test/path/file.name.ext") -- Returns "test/path/file.name"<br/>
	Path("test/path/file.name") -- Returns "test/path/file"<br/>
	Path("test/path/file") -- Returns "test/path/file"
	}}}}
@END]]--
function PathBase(s)
	local e = PathFileExt(s)
	if e == "" then
		return s
	end
	
	return string.sub(s,1,string.len(s)-string.len(PathFileExt(s))-1)
end

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

-- [TODO: Improve]
-- [TODO: Should be in C]
--[[@FUNCTION
	TODO
@END]]--
function PathRelative(base, str)
	local l = string.len(base)
	if string.sub(s, 0, l) == base then
		return string.sub(s, l+2)
	end
	return s
end

--[[@FUNCTION
	TODO
@END]]--
function PathScriptRelative(s)
	return PathRelative(_bam_path, s)
end

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

function NewPathTable()
	local t = NewTable()
	return t
end

function NewAntiPathTable()
	local t = NewTable()
	return t
end

-- copied from the lua-users mailing-list and modified abit 
function table.copy(self, ud_copy_fn)
    ud_copy_fn = ud_copy_fn or function ( ud ) return ud end
    
    local new_table = {}
    for key, value in pairs(self) do
        local new_key
        if(type(key) == 'table') then
            new_key = table.copy(key, ud_copy_fn)
        elseif (type(key) == 'userdata') then
            new_key = ud_copy_fn(key)
        else
            new_key = key
        end

        local new_value
        if(type( value ) == 'table' ) then
            new_value = table.copy(value, ud_copy_fn)
        elseif(type(value) == 'userdata') then
            new_value = ud_copy_fn(value)
        else
            new_value = value
        end

        new_table[new_key] = new_value
    end

     return new_table
end

function table.lock(t)
	local mt = getmetatable(t)
	if not mt then mt = {} end
	mt.__newindex = function(t, key, value)
		error("trying to create key '" .. key .. "' on a locked table")
	end
	setmetatable(t,mt)
end

--[[@GROUP Settings @END]]--

_bam_tools = {}

function AddTool(name, func)
	_bam_tools[name] = func
end

function NewSettings()
	local settings = {}
	
	settings._is_settingsobject = true
	settings.Copy = table.copy
	
	settings.filemappings = {}

	settings.config_name = ""
	settings.config_ext = ""
	settings.debug = 1
	settings.optimize = 0
	
	-- add all tools
	for name, tool in pairs(_bam_tools) do
		tool(settings)
	end

	table.lock(settings)
	
	-- set default drivers
	if family == "windows" then
		SetDriversCL(settings)
	else
		SetDriversGCC(settings)
	end
	
	return settings
end


--[[@GROUP Files and Directories@END]]--

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

--[[@GROUP Actions@END]]--

-- Copy -
--[[@FUNCTION
	TODO
@END]]--
function Copy(outputdir, ...)
	local outputs = {}
	
	-- compile all the files
	for inname in WalkTable({...}) do
		output = Path(outputdir .. "/" .. PathFilename(inname))
		input = Path(inname)

		local copy_command = "cp"
		local srcfile = input
		local dstfile = output
		if family == "windows" then
			copy_command = "copy"
			srcfile = str_replace(srcfile, "/", "\\")
			dstfile = str_replace(dstfile, "/", "\\")
		end

		AddJob(output,
			"copy " .. input .. " -> " .. output,
			copy_command .. " " .. input .. " " .. output)

		AddDependency(output, input)
		table.insert(outputs, output)
	end
	
	return outputs
end

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
	AddJob(name, "", "")
	
	-- all the files
	for inname in WalkTable({...}) do
		AddDependency(name, inname)
	end

	return name
end

--[[@GROUP Configuration@END]]--

--[[@FUNCTION
	TODO
@END]]--
function NewConfig(on_configured_callback)
	local config = {}

	config.OnConfigured = function(self)
		return true
	end
	
	if on_configured_callback then config.OnConfigured = on_configured_callback end
	
	config.options = {}
	config.settings = NewSettings()
	
	config.NewSettings = function(self)
		local s = NewSettings()
		for _,v in pairs(self.options) do
			v:Apply(s)
		end
		return s
	end

	config.Add = function(self, o)
		table.insert(self.options, o)
		self[o.name] = o
	end
	
	config.Print = function(self)
		for k,v in pairs(self.options) do
			print(v:FormatDisplay())
		end
	end
	
	config.Save = function(self, filename)
		print("saved configuration to '"..filename.."'")
		local file = io.open(filename, "w")
		
		-- Define a little helper function to save options
		local saver = {}
		saver.file = file
		
		saver.line = function(self, str)
			self.file:write(str .. "\n")
		end
		
		saver.option = function(self, option, name)
			local valuestr = "no"
			if type(option[name]) == type(0) then
				valuestr = option[name]
			elseif type(option[name]) == type(true) then
				valuestr = "false"
				if option[name] then
					valuestr = "true"
				end
			elseif type(option[name]) == type("") then
				valuestr = "'"..option[name].."'"
			else
				error("option "..name.." have a value of type ".. type(option[name]).." that can't be saved")
			end
			self.file:write(option.name.."."..name.." = ".. valuestr.."\n")
		end

		-- Save all the options		
		for k,v in pairs(self.options) do
			v:Save(saver)
		end
		file:close()
	end
	
	config.Load = function(self, filename)
		local options_func = loadfile(filename)
		local options_table = {}
		
		if options_func then
			-- Setup the options tables
			for k,v in pairs(self.options) do
				options_table[v.name] = {}
			end
			setfenv(options_func, options_table)
			
			-- this is to make sure that we get nice error messages when
			-- someone sets an option that isn't valid.
			local mt = {}
			mt.__index = function(t, key)
				local v = rawget(t, key)
				if v ~= nil then return v end
				error("there is no configuration option named '" .. key .. "'")
			end
			
			setmetatable(options_table, mt)		

			-- Process the options
			options_func()

			-- Copy the options
			for k,v in pairs(self.options) do
				if options_table[v.name] then
					for k2,v2 in pairs(options_table[v.name]) do
						v[k2] = v2
					end
					v.auto_detected = false
				end
			end
		else
			print("error: no '"..filename.."' found")
			print("")
			print("run 'bam config' to generate")
			print("run 'bam config help' for configuration options")
			print("")
			os.exit(1)			
		end
	end

	config.Autodetect = function(self)
		for k,v in pairs(self.options) do
			v:Check(self.settings)
			print(v:FormatDisplay())
			self[v.name] = v
		end
	end

	config.PrintHelp = function(self)
		print("options:")
		for k,v in pairs(self.options) do
			if v.PrintHelp then
				v:PrintHelp()
			end
		end
	end
	
	config.Finalize = function(self, filename)
		if _bam_targets[0] == "config" then
			if _bam_targets[1] == "help" then
				self:PrintHelp()
				os.exit(0)
			end
			
			print("")
			print("configuration:")
			if _bam_targets[1] == "print" then
				self:Load(filename)
				self:Print()
				print("")
				print("notes:")
				self:OnConfigured()
				print("")
			else
				self:Autodetect()
				print("")
				print("notes:")
				if self:OnConfigured() then
					self:Save(filename)
				end
				print("")
			end

			os.exit(0)
		end
	
		self:Load(filename)
		bam_update_globalstamp(filename)
	end
	
	return config
end

--- SilentExecute
function _execute_silent_win(command) return os.execute(command .. " >nul 2>&1") end
function _execute_silent_unix(command) return os.execute(command .. " >/dev/null 2>/dev/null") end

--[[@FUNCTION ExecuteSilent(command)
	TODO
@END]]--

if family == "windows" then
	ExecuteSilent = _execute_silent_win
else
	ExecuteSilent = _execute_silent_unix
end

-- Helper functions --------------------------------------
function DefaultOptionDisplay(option)
	if not option.value then return "no" end
	if option.value == 1 or option.value == true then return "yes" end
	return option.value
end

function IsNegativeTerm(s)
	if s == "no" then return true end
	if s == "false" then return true end
	if s == "off" then return true end
	if s == "disable" then return true end
	if s == "0" then return true end
	return false
end

function IsPositiveTerm(s)
	if s == "yes" then return true end
	if s == "true" then return true end
	if s == "on" then return true end
	if s == "enable" then return true end
	if s == "1" then return true end
	return false
end

function MakeOption(name, value, check, save, display, printhelp)
	local o = {}
	o.name = name
	o.value = value
	o.Check = check
	o.Save = save
	o.auto_detected = true
	o.FormatDisplay = function(self)
		local a = "SET"
		if self.auto_detected then a = "AUTO" end	
		return string.format("%-5s %-20s %s", a, self.name, self:Display())
	end
	
	o.Display = display
	o.PrintHelp = printhelp
	if o.Display == nil then o.Display = DefaultOptionDisplay end
	return o
end


-- Test Compile C --------------------------------------
function OptTestCompileC(name, source, compileoptions, desc)
	local check = function(option, settings)
		option.value = false
		if ScriptArgs[option.name] then
			if IsNegativeTerm(ScriptArgs[option.name]) then
				option.value = false
			elseif IsPositiveTerm(ScriptArgs[option.name]) then
				option.value = true
			else
				error(ScriptArgs[option.name].." is not a valid value for option "..option.name)
			end
			option.auto_detected = false
		else
			if CTestCompile(settings, option.source, option.compileoptions) then
				option.value = true
			end
		end
	end
	
	local save = function(option, output)
		output:option(option, "value")
	end
	
	local printhelp = function(option)
		print("\t"..option.name.."=on|off")
		if option.desc then print("\t\t"..option.desc) end
	end
	
	local o = MakeOption(name, false, check, save, nil, printhelp)
	o.desc = desc
	o.source = source
	o.compileoptions = compileoptions
	return o
end


-- OptToggle --------------------------------------
function OptToggle(name, default_value, desc)
	local check = function(option, settings)
		if ScriptArgs[option.name] then
			if IsNegativeTerm(ScriptArgs[option.name]) then
				option.value = false
			elseif IsPositiveTerm(ScriptArgs[option.name]) then
				option.value = true
			else
				error(ScriptArgs[option.name].." is not a valid value for option "..option.name)
			end
		end
	end
	
	local save = function(option, output)
		output:option(option, "value")
	end
	
	local printhelp = function(option)
		print("\t"..option.name.."=on|off")
		if option.desc then print("\t\t"..option.desc) end
	end
	
	local o = MakeOption(name, default_value, check, save, nil, printhelp)
	o.desc = desc
	return o
end

-- OptInteger --------------------------------------
function OptInteger(name, default_value, desc)
	local check = function(option, settings)
		if ScriptArgs[option.name] then
			option.value = tonumber(ScriptArgs[option.name])
		end
	end
	
	local save = function(option, output)
		output:option(option, "value")
	end
	
	local printhelp = function(option)
		print("\t"..option.name.."=N")
		if option.desc then print("\t\t"..option.desc) end
	end
	
	local o = MakeOption(name, default_value, check, save, nil, printhelp)
	o.desc = desc
	return o
end


-- OptString --------------------------------------
function OptString(name, default_value, desc)
	local check = function(option, settings)
		if ScriptArgs[option.name] then
			option.value = ScriptArgs[option.name]
		end
	end
	
	local save = function(option, output)
		output:option(option, "value")
	end
	
	local printhelp = function(option)
		print("\t"..option.name.."=STRING")
		if option.desc then print("\t\t"..option.desc) end
	end
	
	local o = MakeOption(name, default_value, check, save, nil, printhelp)
	o.desc = desc
	return o
end
-- Find Compiler --------------------------------------
--[[@FUNCTION
	TODO
@END]]--
function OptCCompiler(name, default_driver, default_c, default_cxx, desc)
	local check = function(option, settings)
		if ScriptArgs[option.name] then
			-- set compile driver
			option.driver = ScriptArgs[option.name]

			-- set c compiler
			if ScriptArgs[option.name..".c"] then
				option.c_compiler = ScriptArgs[option.name..".c"]
			end

			-- set c+= compiler
			if ScriptArgs[option.name..".cxx"] then
				option.cxx_compiler = ScriptArgs[option.name..".cxx"]
			end
			
			option.auto_detected = false
		elseif option.driver then
			-- no need todo anything if we have a driver
			-- TODO: test if we can find the compiler
		else
			if ExecuteSilent("g++ -v") == 0 then
				option.driver = "gcc"
			elseif ExecuteSilent("cl") == 0 then
				option.driver = "cl"
			else
				error("no c/c++ compiler found")
			end
		end
		--setup_compiler(option.value)
	end

	local apply = function(option, settings)
		if option.driver == "cl" then
			SetDriversCL(settings)
		elseif option.driver == "gcc" then
			SetDriversGCC(settings)
		else
			error(option.driver.." is not a known c/c++ compile driver")
		end
		
		if option.c_compiler then settings.cc.c_compiler = option.c_compiler end
		if option.cxx_compiler then settings.cc.cxx_compiler = option.cxx_compiler end
	end
	
	local save = function(option, output)
		output:option(option, "driver")
		output:option(option, "c_compiler")
		output:option(option, "cxx_compiler")
	end

	local printhelp = function(option)
		local a = ""
		if option.desc then a = "for "..option.desc end
		print("\t"..option.name.."=gcc|cl")
		print("\t\twhat c/c++ compile driver to use"..a)
		print("\t"..option.name..".c=FILENAME")
		print("\t\twhat c compiler executable to use"..a)
		print("\t"..option.name..".cxx=FILENAME")
		print("\t\twhat c++ compiler executable to use"..a)
	end

	local display = function(option)
		local s = option.driver
		if option.c_compiler then s = s .. " c="..option.c_compiler end
		if option.cxx_compiler then s = s .. " cxx="..option.cxx_compiler end
		return s
	end
		
	local o = MakeOption(name, nil, check, save, display, printhelp)
	o.desc = desc
	o.driver = false
	o.c_compiler = false
	o.cxx_compiler = false

	if default_driver then o.driver = default_driver end
	if default_c then o.c_compiler = default_c end
	if default_cxx then o.cxx_compiler = default_cxx end

	o.Apply = apply
	return o
end

-- Option Library --------------------------------------
--[[@FUNCTION
	TODO
@END]]--
function OptLibrary(name, header, desc)
	local check = function(option, settings)
		option.value = false
		option.include_path = false
		
		local function check_compile_include(filename, paths)
			if CTestCompile(settings, "#include <" .. filename .. ">\nint main(){return 0;}", "") then
				return ""
			end

			for k,v in pairs(paths) do
				if CTestCompile(settings, "#include <" .. filename .. ">\nint main(){return 0;}", "-I"..v) then
					return v
				end
			end
			
			return false
		end

		if ScriptArgs[option.name] then
			if IsNegativeTerm(ScriptArgs[option.name]) then
				option.value = false
			elseif ScriptArgs[option.name] == "system" then
				option.value = true
			else
				option.value = true
				option.include_path = ScriptArgs[option.name]
			end
			option.auto_detected = false
		else
			option.include_path = check_compile_include(option.header, {})
			if option.include_path == false then
				if option.required then
					print(name.." library not found and is required")
					error("required library not found")
				end
			else
				option.value = true
				option.include_path = false
			end
		end
	end
	
	local save = function(option, output)
		output:option(option, "value")
		output:option(option, "include_path")
	end
	
	local display = function(option)
		if option.value then
			if option.include_path then
				return option.include_path
			else
				return "(in system path)"
			end
		else
			return "not found"
		end
	end

	local printhelp = function(option)
		print("\t"..option.name.."=disable|system|PATH")
		if option.desc then print("\t\t"..option.desc) end
	end
	
	local o = MakeOption(name, false, check, save, display, printhelp)
	o.include_path = false
	o.header = header
	o.desc = desc
	return o
end

---

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
	add extra parameters for dependencies for the job
@END]]--
-- TODO: Implement this in C
function AddJob(output, label, command, ...)
	bam_add_job(output, label, command)
	AddDependency(output, {...})
end

--[[@FUNCTION AddDependency(filename, depfilename)
	Specifies a dependency of a file. The file specified in the
	^depfilename^ is a dependency of ^filename^.
@END]]--
-- TODO: Implement this in C
function AddDependency(filename, ...)
	for f in WalkTable({...}) do
		bam_add_dependency(filename, f)
	end
end
-- AddDependency = bam_add_dependency


function Default_Intermediate_Output(settings, input)
	return PathBase(input) .. settings.config_ext
end

function DriverNull()
	error("no driver set")
end

------------------------ C/C++ COMPILE ------------------------

function InitCommonCCompiler(settings)
	settings.cc = {}
	settings.cc.extension = ""
	settings.cc.path = ""
	settings.cc.c_exe = ""
	settings.cc.cxx_exe = ""
	settings.cc.DriverCTest = DriverNull
	settings.cc.DriverC = DriverNull
	settings.cc.DriverCXX = DriverNull
	settings.cc.flags = NewFlagTable()
	settings.cc.includes = NewPathTable()
	settings.cc.systemincludes = NewTable()
	settings.cc.defines = NewTable()
	settings.cc.frameworks = NewTable()
	settings.cc.Output = Default_Intermediate_Output
	settings.cc.optimize = 0
	
	table.lock(settings.cc)
end

function CCompiler(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	AddJob(
		outname,
		"c " .. input,
		settings.cc.DriverC(outname, input, settings)
	)	
	AddDependency(outname, input)
	bam_dependency_cpp(input, settings.cc.includes)
	return outname
end

function CXXCompiler(settings, input)
	local outname = settings.cc.Output(settings, input) .. settings.cc.extension
	AddJob(
		outname,
		"c++ " .. input,
		settings.cc.DriverCXX(outname, input, settings)
	)	
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
	
	for inname in WalkTable({...}) do
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

------------------------ LINK ------------------------

AddTool("link", function (settings)
	settings.link = {}
	settings.link.Driver = DriverNull
	settings.link.path = ""
	settings.link.extension = ""
	settings.link.exe = ""
	settings.link.inputflags = ""
	settings.link.flags = NewFlagTable()
	settings.link.libs = NewTable()
	settings.link.frameworks = NewTable()
	settings.link.frameworkpath = NewPathTable()
	settings.link.libpath = NewPathTable()
	settings.link.extrafiles = NewPathTable()
	
	table.lock(settings.link)
end)

--[[@FUNCTION
	TODO
@END]]--
function Link(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = FlattenTable({...})

	output = output .. settings.config_ext .. settings.link.extension

	AddJob(output, "link " .. output, settings.link.Driver(output, inputs, settings))

	-- all the files
	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end
	
	for index, inname in ipairs(settings.link.extrafiles) do
		AddDependency(output, inname)
	end

	return output
end

------------------------ STATIC LIBRARY ACTION ------------------------

AddTool("lib", function (settings)
	settings.lib = {}
	settings.lib.Driver = DriverNull
	settings.lib.extension = ""
	settings.lib.path = ""
	settings.lib.exe = ""
	settings.lib.flags = NewFlagTable()
	
	table.lock(settings.lib)
end)

--[[@FUNCTION
	TODO
@END]]--
function StaticLibrary(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = FlattenTable({...})

	output = output .. settings.config_ext .. settings.lib.extension

	AddJob(output, "link " .. output, settings.lib.Driver(output, inputs, settings))

	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end

	return output
end

------------------------ SHARED LIBRARY ACTION ------------------------

AddTool("dll", function (settings)
	settings.dll = {}
	settings.dll.Driver = DriverNull
	settings.dll.extension = ""
	settings.dll.path = ""
	settings.dll.exe = ""
	settings.dll.inputflags = ""
	settings.dll.flags = NewFlagTable()
	settings.dll.libs = NewTable()
	settings.dll.frameworks = NewTable()
	settings.dll.frameworkpath = NewPathTable()
	settings.dll.libpath = NewPathTable()
	settings.dll.extrafiles = NewPathTable()

	table.lock(settings.dll)
end)

--[[@FUNCTION
	TODO
@END]]--
function SharedLibrary(settings, output, ...)
	CheckSettings(settings)
	
	local inputs = FlattenTable({...})

	output = output .. settings.config_ext .. settings.dll.extension
	AddJob(output, "dll ".. output, settings.dll.Driver(output, inputs, settings))

	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end

	return output
end

