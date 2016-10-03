function DriverSolarisStudio_CTest(code, options)
	local f = io.open("_test.c", "w")
	f:write(code)
	f:write("\n")
	f:close()
	local ret = ExecuteSilent("cc _test.c -o _test " .. options)
	os.remove("_test.c")
	os.remove("_test")
	return ret==0
end

function SetDriversSolarisStudio(settings)
	SetDriversGCC(settings)

	if settings.cc then
		settings.cc.exe_c = "suncc"
		settings.cc.exe_cxx = "sunCC"
		settings.cc.DriverCTest = DriverSolarisStudio_CTest
	end

	if settings.link then
		settings.link.exe = "sunCC"
	end

	if settings.dll then
		settings.dll.exe = "sunCC"
	end
end
