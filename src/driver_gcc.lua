
------------------------ C/C++ GCC DRIVER ------------------------
function DriverGCC_Get(exe, cache_name, flags_name)
	return function(label, output, input, settings)
		local cache = settings.cc[cache_name]
		if settings.invoke_count ~= cache.nr then
			cache.nr = settings.invoke_count
			local cc = settings.cc
			local d = TableToString(cc.defines, "-D", " ")
			local i = TableToString(cc.includes, '-I "', '" ')
			local i = i .. TableToString(cc.systemincludes, '-isystem "', '" ')
			local i = i .. TableToString(cc.frameworks, '-framework ', ' ')
			local f = cc.flags:ToString()
			f = f .. cc[flags_name]:ToString()
			if settings.debug > 0 then f = f .. "-g " end
			if settings.optimize > 0 then f = f .. "-O2 " end
			
			cache.str = cc[exe] .. " " .. f .. "-c " .. d .. i .. " -o "
		end
		
		AddJob(output, label, cache.str .. output .. " " .. input)
	end
end

function DriverGCC_CTest(code, options)
	local f = io.open("_test.c", "w")
	f:write(code)
	f:write("\n")
	f:close()
	local ret = ExecuteSilent("gcc _test.c -o _test " .. options)
	os.remove("_test.c")
	os.remove("_test")
	return ret==0
end

------------------------ LINK GCC DRIVER ------------------------

function DriverGCC_Link(label, output, inputs, settings)
	local e = settings.link.exe .. " -o " .. output
	local e = e .. " " .. settings.link.inputflags .. " " .. TableToString(inputs, '', ' ') 
	local e = e .. TableToString(settings.link.extrafiles, '', ' ')
	local e = e .. TableToString(settings.link.libpath, '-L', ' ')
	local e = e .. TableToString(settings.link.libs, '-l', ' ')
	local e = e .. TableToString(settings.link.frameworkpath, '-F', ' ')
	local e = e .. TableToString(settings.link.frameworks, '-framework ', ' ')
	local e = e .. settings.link.flags:ToString()
	AddJob(output, label, e)
end

------------------------ LIB GCC DRIVER ------------------------

function DriverGCC_Lib(output, inputs, settings)
	local e = settings.lib.exe .. " rcu " .. output
	local e = e .. " " .. TableToString(inputs, '', ' ') .. settings.lib.flags:ToString()
	return e
end

------------------------ DLL GCC DRIVER ------------------------

function DriverGCC_DLL(label, output, inputs, settings)
	local shared_flags = ""

	if platform == "macosx" then
		shared_flags = " -dynamiclib"
	else
		shared_flags = " -shared"
	end

	local e = settings.dll.exe .. shared_flags .. " -o " .. output
	local e = e .. " " .. settings.dll.inputflags .. " " .. TableToString(inputs, '', ' ') 
	local e = e .. TableToString(settings.dll.extrafiles, '', ' ')
	local e = e .. TableToString(settings.dll.libpath, '-L', ' ')
	local e = e .. TableToString(settings.dll.libs, '-l', ' ')
	local e = e .. TableToString(settings.dll.frameworkpath, '-F', ' ')
	local e = e .. TableToString(settings.dll.frameworks, '-framework ', ' ')
	local e = e .. settings.dll.flags:ToString()
	AddJob(output, label, e)
end

function SetDriversGCC(settings)
	if settings.cc then
		settings.cc.extension = ".o"
		settings.cc.exe_c = "gcc"
		settings.cc.exe_cxx = "g++"
		settings.cc.DriverCTest = DriverGCC_CTest
		settings.cc.DriverC = DriverGCC_Get("exe_c", "_c_cache", "flags_c")
		settings.cc.DriverCXX = DriverGCC_Get("exe_cxx", "_cxx_cache", "flags_cxx")
	end
	
	if settings.link then
		if family == "windows" then
			settings.link.extension = ".exe"
		else
			settings.link.extension = ""
		end
		settings.link.exe = "g++"
		settings.link.Driver = DriverGCC_Link
	end
	
	if settings.lib then
		settings.lib.prefix = "lib"
		settings.lib.extension = ".a"
		settings.lib.exe = "ar"
		settings.lib.Driver = DriverGCC_Lib
	end
	
	if settings.dll then
		if platform == "macosx" then
			settings.dll.prefix = "lib"
			settings.dll.extension = ".dylib"
		else
			settings.dll.prefix = ""
			settings.dll.extension = ".so"
		end
		settings.dll.exe = "g++"
		settings.dll.Driver = DriverGCC_DLL
	end
end
