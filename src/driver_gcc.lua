
------------------------ C/C++ GCC DRIVER ------------------------

function compile_c_cxx_gcc(label, exe, output, input, settings)
	local d = tbl_to_str(settings.cc.defines, "-D", " ")
	local i = tbl_to_str(settings.cc.includes, '-I "', '" ')
	local i = i .. tbl_to_str(settings.cc.systemincludes, '-isystem "', '" ')
	local i = i .. tbl_to_str(settings.cc.frameworks, '-framework ', ' ')
	local f = settings.cc.flags:ToString()
	if settings.debug > 0 then f = f .. "-g " end
	if settings.optimize > 0 then f = f .. "-O2 " end
	local e = exe .. ' ' .. f ..'-c ' .. input .. ' -o ' .. output .. ' ' .. d .. i
	return e
	-- return bam_execute(e)
end

function DriverCXX_GCC(output,input,settings)
	return compile_c_cxx_gcc("c++ " .. PathFilename(input), settings.cc.cxx_exe,output,input,settings)
end

function DriverC_GCC(output,input,settings)
	return compile_c_cxx_gcc("c " .. PathFilename(input), settings.cc.c_exe,output,input,settings)
end

function DriverCTest_GCC(code, options)
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

function DriverLink_GCC(output, inputs, settings)
	local e = settings.link.exe .. " -o " .. output
	local e = e .. " " .. settings.link.inputflags .. " " .. tbl_to_str(inputs, '', ' ') 
	local e = e .. tbl_to_str(settings.link.extrafiles, '', ' ')
	local e = e .. tbl_to_str(settings.link.libpath, '-L', ' ')
	local e = e .. tbl_to_str(settings.link.libs, '-l', ' ')
	local e = e .. tbl_to_str(settings.link.frameworkpath, '-F', ' ')
	local e = e .. tbl_to_str(settings.link.frameworks, '-framework ', ' ')
	local e = e .. settings.link.flags:ToString()
	return e
end

------------------------ LIB GCC DRIVER ------------------------

function DriverLib_GCC(output, inputs, settings)
	local e = settings.lib.exe .. " rcu " .. output
	local e = e .. " " .. tbl_to_str(inputs, '', ' ') .. settings.lib.flags:ToString()
	return e
end

------------------------ DLL GCC DRIVER ------------------------

function DriverDLL_GCC(output, inputs, settings)
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
	return e
end

function SetDriversGCC(settings)
	if settings.cc then
		settings.cc.extension = ".o"
		settings.cc.c_exe = "gcc"
		settings.cc.cxx_exe = "g++"
		settings.cc.DriverCTest = DriverCTest_GCC
		settings.cc.DriverC = DriverC_GCC
		settings.cc.DriverCXX = DriverCXX_GCC	
	end
	
	if settings.link then
		settings.link.extension = ""
		settings.link.exe = "g++"
		settings.link.Driver = DriverLink_GCC
	end
	
	if settings.lib then
		settings.lib.prefix = "lib"
		settings.lib.extension = ".a"
		settings.lib.exe = "ar"
		settings.lib.Driver = DriverLib_GCC
	end
	
	if settings.dll then
		settings.dll.extension = ".so"
		settings.dll.exe = "g++"
		settings.dll.Driver = DriverDLL_GCC
	end
end
