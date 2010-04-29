
----- cl compiler ------
function DriverCL_Common(cpp, settings)
	local defs = TableToString(settings.cc.defines, "-D", " ") .. " "
	local incs = TableToString(settings.cc.includes, '-I"', '" ')
	local incs = incs .. TableToString(settings.cc.systemincludes, '-I"', '" ')
	local flags = settings.cc.flags:ToString()
	if cpp then
		flags = flags .. settings.cc.flags_cxx:ToString()
	else
		flags = flags .. settings.cc.flags_c:ToString()
	end
	
	local exe = str_replace(settings.cc.exe_c, "/", "\\")
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
	local cache = settings.cc._cxx_cache
	if settings.invoke_count ~= cache.nr then
		cache.nr = settings.invoke_count
		cache.str = DriverCL_Common(true, settings)
	end
	
	AddJob(output, label, cache.str .. output .. " " .. input)
	SetFilter(output, "F" .. PathFilename(input))
end

function DriverCL_C(label, output, input, settings)
	local cache = settings.cc._c_cache
	if settings.invoke_count ~= cache.nr then
		cache.nr = settings.invoke_count
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
	local input =  TableToString(inputs, "", " ")
	local exe = str_replace(settings.lib.exe, "/", "\\")
	local exec = exe .. " /nologo " .. settings.lib.flags:ToString() .. " /OUT:" .. output
	return DriverCL_BuildResponse(exec, output, input)
end

function DriverCL_Link_Common(label, output, inputs, settings, part, extra)
	local input =  TableToString(inputs, "", " ") .. TableToString(part.extrafiles, "", " ")
	local flags = part.flags:ToString()
	local libs  = TableToString(part.libs, "", ".lib ")
	local libpaths = TableToString(part.libpath, "/libpath:\"", "\" ")
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
		settings.cc.exe_c = "cl"
		settings.cc.exe_cxx = "cl"
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
