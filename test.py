#!/usr/bin/python

import os, sys, shutil

extra_bam_flags = ""
src_path = "tests"
output_path = "test_output"

failed_tests = []

tests = []


bam = "../../src/bam"
if os.name == 'nt':
	bam = "..\\..\\src\\bam"

if len(sys.argv) > 1:
	tests = sys.argv[1:]

def copytree(src, dst):
	names = os.listdir(src)
	os.mkdir(dst)
	for name in names:
		if name[0] == '.':
			continue
		srcname = os.path.join(src, name)
		dstname = os.path.join(dst, name)
		
		try:
			if os.path.isdir(srcname):
				copytree(srcname, dstname)
			else:
				shutil.copy2(srcname, dstname)
		except (IOError, os.error), why:
			print "Can't copy %s to %s: %s" % (`srcname`, `dstname`, str(why))


def test(name, moreflags="", should_fail=0):
	global output_path, failed_tests, tests

	if len(tests) and not name in tests:
		return

	print "-- " + name.upper() + " --"
	olddir = os.getcwd()
	os.chdir(output_path+"/"+name)
	cmdline = bam+" "+extra_bam_flags+" " + moreflags
	print cmdline
	ret = os.system(cmdline)
	print "return code =", ret
	print "----"
	print ""

	os.chdir(olddir)
	
	if should_fail and not ret:
		failed_tests += [name]
	elif not should_fail and ret:
		failed_tests += [name]
		

# clean
shutil.rmtree(output_path, True)

# copy tree
copytree("tests", output_path)

# run tests
test("cyclic")
test("include_paths")
test("clone")
test("dot.in.dir")
#test("subproject")
test("retval", "", 1)
test("multi_target", "SHOULD_NOT_EXIST", 1)
test("multi_target", "CORRECT_ONE")
test("collect_wrong", "", 1)
test("locked", "", 1)

if len(failed_tests):
	print "FAILED TESTS:"
	for t in failed_tests:
		print "\t"+t
else:
	print "ALL TESTS PASSED!"

