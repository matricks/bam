
----- cl compiler ------
function compile_c_cxx_cl(output, input, settings, label)
	local defs = tbl_to_str(settings.cc.defines, "-D", " ") .. " "
	local incs = tbl_to_str(settings.cc.includes, '-I"', '" ')
	local incs = incs .. tbl_to_str(settings.cc.systemincludes, '-I"', '" ')
	local flags = settings.cc.flags:ToString()
	local path = str_replace(settings.cc.path, "/", "\\")
	if platform =="win32" then
		flags = flags .. " /D \"WIN32\" "
	else
		flags = flags .. " /D \"WIN64\" "
	end
	if settings.debug > 0 then flags = flags .. "/Od /MTd /Zi /D \"_DEBUG\" " end
	if settings.optimize > 0 then flags = flags .. "/Ox /Ot /MT /D \"NDEBUG\" " end
	local exec = path .. 'cl /nologo /D_CRT_SECURE_NO_DEPRECATE /c ' .. flags .. input .. " " .. incs .. defs .. " /Fo" .. output
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
	local path = str_replace(settings.lib.path, "/", "\\")
	local exec = path .. 'lib /nologo ' .. settings.lib.flags:ToString() .. " /OUT:" .. output .. " " .. input
	return exec
end

function DriverDLL_CL(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local flags = settings.dll.flags:ToString()
	local libs  = tbl_to_str(settings.dll.libs, "", " ")
	local libpaths = tbl_to_str(settings.dll.libpath, "/libpath:\"", "\" ")
	local path = str_replace(settings.path, "/", "\\")
	local exec = path .. 'link /nologo /DLL' .. flags .. libpaths .. libs .. " /OUT:" .. output .. " " .. input
	return exec
end

function DriverLink_CL(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local flags = settings.link.flags:ToString()
	local libs  = tbl_to_str(settings.link.libs, "", ".lib ")
	local libpaths = tbl_to_str(settings.link.libpath, "/libpath:\"", "\" ")
	local path = str_replace(settings.link.path, "/", "\\")
	if settings.debug > 0 then flags = flags .. "/DEBUG " end
	local exec = path .. 'link /nologo /incremental:no ' .. flags .. libpaths .. libs .. " /OUT:" .. output .. " " .. input
	return exec
end

function SetDriversCL(settings)
	if settings.cc then
		settings.cc.extension = ".obj"
		settings.cc.c_compiler = "cl"
		settings.cc.cxx_compiler = "cl"
		settings.cc.DriverCTest = DriverCTest_CL
		settings.cc.DriverC = DriverC_CL
		settings.cc.DriverCXX = DriverCXX_CL	
	end
	
	if settings.link then
		settings.link.extension = ".exe"
		settings.link.Driver = DriverLink_CL
	end
	
	if settings.lib then
		settings.lib.extension = ".lib"
		settings.lib.Driver = DriverLib_CL
	end
	
	if settings.dll then
		settings.dll.extension = ".dll"
		settings.dll.Driver = DriverDLL_CL
	end
end
