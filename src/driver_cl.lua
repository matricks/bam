
----- cl compiler ------
function DriverCL_Common(cpp, settings)
	local defs = tbl_to_str(settings.cc.defines, "-D", " ") .. " "
	local incs = tbl_to_str(settings.cc.includes, '-I"', '" ')
	local incs = incs .. tbl_to_str(settings.cc.systemincludes, '-I"', '" ')
	local flags = settings.cc.flags:ToString()
	if cpp then
		flags = flags .. settings.cc.cpp_flags:ToString()
	else
		flags = flags .. settings.cc.c_flags:ToString()
	end
	
	local exe = str_replace(settings.cc.c_exe, "/", "\\")
	if platform =="win32" then
		flags = flags .. " /D \"WIN32\" "
	else
		flags = flags .. " /D \"WIN64\" "
	end
	
	if settings.debug > 0 then flags = flags .. "/Od /MTd /Z7 /D \"_DEBUG\" " end
	if settings.optimize > 0 then flags = flags .. "/Ox /Ot /MT /D \"NDEBUG\" " end
	local exec = exe .. " /nologo /D_CRT_SECURE_NO_DEPRECATE /c " .. flags .. " " .. incs .. defs .. " /Fo"
	return exec
end

function DriverCL_CXX(label, output,input, settings)
	local cc = settings.cc
	local cache = cc._cxx_cache
	if cc._invoke_counter ~= cache.nr then
		cache.nr = settings.cc._invoke_counter
		cache.str = DriverCL_Common(true, settings)
	end
	
	AddJob(output, label, cache.str .. output .. " " .. input)
	SetFilter(output, "F" .. PathFilename(input))
end

function DriverCL_C(label, output, input, settings)
	local cc = settings.cc
	local cache = cc._c_cache
	if cc._invoke_counter ~= cache.nr then
		cache.nr = settings.cc._invoke_counter
		cache.str = DriverCL_Common(nil, settings)
	end
	
	AddJob(output, label, cache.str .. output .. " " .. input)
	SetFilter(output, "F" .. PathFilename(input))
end

function DriverCL_CTest(code, options)
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

function DriverCL_BuildResponse(exec, output, input)
	if string.len(exec) + string.len(input) < 8000 then
		return exec .. " " .. input
	else
		local response_filename = output .. ".resp"
		local response_file = io.open(response_filename, "w")
		response_file:write(input)
		response_file:close()
		return exec .. " @" .. response_filename
	end
end

function DriverCL_Lib(output, inputs, settings)
	local input =  tbl_to_str(inputs, "", " ")
	local exe = str_replace(settings.lib.exe, "/", "\\")
	local exec = exe .. " /nologo " .. settings.lib.flags:ToString() .. " /OUT:" .. output
	return DriverCL_BuildResponse(exec, output, input)
end

function DriverCL_Link_Common(label, output, inputs, settings, part, extra)
	local input =  tbl_to_str(inputs, "", " ")
	local flags = part.flags:ToString()
	local libs  = tbl_to_str(part.libs, "", ".lib ")
	local libpaths = tbl_to_str(part.libpath, "/libpath:\"", "\" ")
	local exe = str_replace(part.exe, "/", "\\")
	if settings.debug > 0 then flags = flags .. "/DEBUG " end
	local exec = exe .. " /nologo /incremental:no " .. extra .. " " .. flags .. libpaths .. libs .. " /OUT:" .. output
	exec = DriverCL_BuildResponse(exec, output, input)
	AddJob(output, label, exec)
end

function DriverCL_DLL(label, output, inputs, settings)
	DriverCL_Link_Common(label, output, inputs, settings, settings.dll, "/DLL")
	local libfile = string.sub(output, 0, string.len(output) - string.len(settings.dll.extension)) .. settings.lib.extension
	AddOutput(output, libfile)
end

function DriverCL_Link(label, output, inputs, settings)
	DriverCL_Link_Common(label, output, inputs, settings, settings.link, "")
end

function SetDriversCL(settings)
	if settings.cc then
		settings.cc.extension = ".obj"
		settings.cc.c_exe = "cl"
		settings.cc.cxx_exe = "cl"
		settings.cc.DriverCTest = DriverCL_CTest
		settings.cc.DriverC = DriverCL_C
		settings.cc.DriverCXX = DriverCL_CXX
	end
	
	if settings.link then
		settings.link.extension = ".exe"
		settings.link.exe = "link"
		settings.link.Driver = DriverCL_Link
	end
	
	if settings.lib then
		settings.lib.extension = ".lib"
		settings.lib.exe = "lib"
		settings.lib.Driver = DriverCL_Lib
	end
	
	if settings.dll then
		settings.dll.extension = ".dll"
		settings.dll.exe = "link"
		settings.dll.Driver = DriverCL_DLL
	end
end
