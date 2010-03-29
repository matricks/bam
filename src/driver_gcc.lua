
------------------------ C/C++ GCC DRIVER ------------------------

function DriverGCC_Common(cpp, label, cache, exe, output, input, settings)
	if settings.invoke_count ~= cache.nr then
		cache.nr = settings.invoke_count
		local cc = settings.cc
		local d = tbl_to_str(cc.defines, "-D", " ")
		local i = tbl_to_str(cc.includes, '-I "', '" ')
		local i = i .. tbl_to_str(cc.systemincludes, '-isystem "', '" ')
		local i = i .. tbl_to_str(cc.frameworks, '-framework ', ' ')
		local f = cc.flags:ToString()
		if cpp then
			f = f .. cc.cpp_flags:ToString()
		else
			f = f .. cc.c_flags:ToString()
		end
		if settings.debug > 0 then f = f .. "-g " end
		if settings.optimize > 0 then f = f .. "-O2 " end
		
		cache.str = exe .. " " .. f .. "-c " .. d .. i .. " -o "
	end
	
	AddJob(output, label, cache.str .. output .. " " .. input)
end

function DriverGCC_CXX(label, output, input, settings)
	DriverGCC_Common(true, label, settings.cc._cxx_cache, settings.cc.cxx_exe, output, input, settings)
end

function DriverGCC_C(label, output, input, settings)
	DriverGCC_Common(nil, label, settings.cc._c_cache, settings.cc.c_exe, output, input, settings)
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
	local e = e .. " " .. settings.link.inputflags .. " " .. tbl_to_str(inputs, '', ' ') 
	local e = e .. tbl_to_str(settings.link.extrafiles, '', ' ')
	local e = e .. tbl_to_str(settings.link.libpath, '-L', ' ')
	local e = e .. tbl_to_str(settings.link.libs, '-l', ' ')
	local e = e .. tbl_to_str(settings.link.frameworkpath, '-F', ' ')
	local e = e .. tbl_to_str(settings.link.frameworks, '-framework ', ' ')
	local e = e .. settings.link.flags:ToString()
	AddJob(output, label, e)
end

------------------------ LIB GCC DRIVER ------------------------

function DriverGCC_Lib(output, inputs, settings)
	local e = settings.lib.exe .. " rcu " .. output
	local e = e .. " " .. tbl_to_str(inputs, '', ' ') .. settings.lib.flags:ToString()
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
	local e = e .. " " .. settings.dll.inputflags .. " " .. tbl_to_str(inputs, '', ' ') 
	local e = e .. tbl_to_str(settings.dll.extrafiles, '', ' ')
	local e = e .. tbl_to_str(settings.dll.libpath, '-L', ' ')
	local e = e .. tbl_to_str(settings.dll.libs, '-l', ' ')
	local e = e .. tbl_to_str(settings.dll.frameworkpath, '-F', ' ')
	local e = e .. tbl_to_str(settings.dll.frameworks, '-framework ', ' ')
	local e = e .. settings.dll.flags:ToString()
	AddJob(output, label, e)
end

function SetDriversGCC(settings)
	if settings.cc then
		settings.cc.extension = ".o"
		settings.cc.c_exe = "gcc"
		settings.cc.cxx_exe = "g++"
		settings.cc.DriverCTest = DriverGCC_CTest
		settings.cc.DriverC = DriverGCC_C
		settings.cc.DriverCXX = DriverGCC_CXX	
	end
	
	if settings.link then
		settings.link.extension = ""
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
