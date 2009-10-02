
----- cl compiler ------
function compile_c_cxx_cl(output, input, settings, label)
	local defs = tbl_to_str(settings.cc.defines, "-D", " ") .. " "
	local incs = tbl_to_str(settings.cc.includes, '-I"', '" ')
	local incs = incs .. tbl_to_str(settings.cc.systemincludes, '-I"', '" ')
	local flags = settings.cc.flags:ToString()
	local exe = str_replace(settings.cc.c_exe, "/", "\\")
	if platform =="win32" then
		flags = flags .. " /D \"WIN32\" "
	else
		flags = flags .. " /D \"WIN64\" "
	end
	if settings.debug > 0 then flags = flags .. "/Od /MTd /Zi /D \"_DEBUG\" " end
	if settings.optimize > 0 then flags = flags .. "/Ox /Ot /MT /D \"NDEBUG\" " end
	local exec = exe .. " /nologo /D_CRT_SECURE_NO_DEPRECATE /c " .. flags .. input .. " " .. incs .. defs .. " /Fo" .. output .. " |" .. _bam_exe .. " --filter-matchfirst " .. PathFilename(input)
	return exec
end

function DriverCXX_CL(output,input,settings)
	return compile_c_cxx_cl(output,input,settings,"c++ ")
end

function DriverC_CL(output,input,settings)
	return compile_c_cxx_cl(output,input,settings,"c ")
end

function DriverCTest_CL(code, options)
	local f = io.open("_test.c", "w")
	f:write(code)
	f:write("\n")
	f:close()
	local ret = ExecuteSilent("cl _test.c /Fe_test " .. options)
	os.remove("_test.c")
	os.remove("_test.exe")
	os.remove("_test.obj")
	return ret==0
end

function DriverLib_CL(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local exe = str_replace(settings.lib.exe, "/", "\\")
	local exec = exe .. ' /nologo ' .. settings.lib.flags:ToString() .. " /OUT:" .. output .. " " .. input
	return exec
end

function DriverDLL_CL(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local flags = settings.dll.flags:ToString()
	local libs  = tbl_to_str(settings.dll.libs, "", " ")
	local libpaths = tbl_to_str(settings.dll.libpath, "/libpath:\"", "\" ")
	local exe = str_replace(settings.exe, "/", "\\")
	local exec = exe .. ' /nologo /DLL' .. flags .. libpaths .. libs .. " /OUT:" .. output .. " " .. input
	return exec
end

function DriverLink_CL(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local flags = settings.link.flags:ToString()
	local libs  = tbl_to_str(settings.link.libs, "", ".lib ")
	local libpaths = tbl_to_str(settings.link.libpath, "/libpath:\"", "\" ")
	local exe = str_replace(settings.link.exe, "/", "\\")
	if settings.debug > 0 then flags = flags .. "/DEBUG " end
	local exec = exe .. " /nologo /incremental:no /OUT:" .. output .. " " .. flags .. libpaths .. libs .. " " .. input
	return exec
end

function SetDriversCL(settings)
	if settings.cc then
		settings.cc.extension = ".obj"
		settings.cc.c_exe = "cl"
		settings.cc.cxx_exe = "cl"
		settings.cc.DriverCTest = DriverCTest_CL
		settings.cc.DriverC = DriverC_CL
		settings.cc.DriverCXX = DriverCXX_CL	
	end
	
	if settings.link then
		settings.link.extension = ".exe"
		settings.link.exe = "link"
		settings.link.Driver = DriverLink_CL
	end
	
	if settings.lib then
		settings.lib.extension = ".lib"
		settings.lib.exe = "lib"
		settings.lib.Driver = DriverLib_CL
	end
	
	if settings.dll then
		settings.dll.extension = ".dll"
		settings.dll.exe = "link"
		settings.dll.Driver = DriverDLL_CL
	end
end
