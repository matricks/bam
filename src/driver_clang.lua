function DriverClang_CTest(code, options)
	local f = io.open("_test.c", "w")
	f:write(code)
	f:write("\n")
	f:close()
	local ret = ExecuteSilent("clang _test.c -o _test " .. options)
	os.remove("_test.c")
	os.remove("_test")
	return ret==0
end

function SetDriversClang(settings)
	SetDriversGCC(settings)

	if settings.cc then
		settings.cc.exe_c = "clang"
		settings.cc.exe_cxx = "clang++"
		settings.cc.DriverCTest = DriverClang_CTest
	end

	if settings.link then
		settings.link.exe = "clang++"
	end

	if settings.dll then
		settings.dll.exe = "clang++"
	end
end
