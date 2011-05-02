#!/usr/bin/env python

import os, sys, shutil, subprocess

extra_bam_flags = ""
src_path = "tests"
output_path = "test_output"

failed_tests = []

tests = []
verbose = False

for v in sys.argv:
	if v == "-v":
		verbose = True

bam = "../../bam"
if os.name == 'nt':
	bam = "..\\..\\bam"

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


def run_bam(testname, flags):
	global output_path
	olddir = os.getcwd()
	os.chdir(output_path+"/"+testname)
	
	p = subprocess.Popen(bam+" "+flags, stdout=subprocess.PIPE, shell=True, stderr=subprocess.STDOUT)
	report = p.stdout.readlines()
	p.wait()
	ret = p.returncode
	os.chdir(olddir)
	
	return (ret, report)
	

def test(name, moreflags="", should_fail=0):
	global output_path, failed_tests, tests

	if len(tests) and not name in tests:
		return

	olddir = os.getcwd()
	os.chdir(output_path+"/"+name)
	cmdline = bam+" -t -v "+extra_bam_flags+" " + moreflags
	
	print name + ":",
	p = subprocess.Popen(cmdline, stdout=subprocess.PIPE, shell=True, stderr=subprocess.STDOUT)
	report = p.stdout.readlines()
	p.wait()
	ret = p.returncode
	
	os.chdir(olddir)
	
	if (should_fail and not ret) or (not should_fail and ret):
		print " FAILED!"
		for l in report:
			print "\t", l,
		failed_tests += [name + "(returned %d)" % ret]
	else:
		print " ok"

def difftest(name, flags1, flags2):
	global failed_tests
	if len(tests) and not name in tests:
		return
	testname = "difftest: %s '%s' vs '%s': "%(name, flags1, flags2)
	print testname,
	ret1, report1 = run_bam(name, flags1)
	ret2, report2 = run_bam(name, flags2)
	
	if ret1:
		print "FAILED! '%s' returned %d" %(flags1, ret1)
		failed_tests += [testname]
		return
	
	if ret2:
		print "FAILED! '%s' returned %d" %(flags2, ret2)
		failed_tests += [testname]
		return
	
	if len(report1) != len(report2):
		print "FAILED! %d lines vs %d lines" % (len(report1), len(report2))
		failed_tests += [testname]
		return
	
	failed = 0
	for i in xrange(0, len(report1)):
		if report1[i] != report2[i]:
			if not failed:
				print "FAILED!"
			print "1:", report1[i].strip()
			print "2:", report2[i].strip()
			failed += 1
			
	if failed:
		failed_tests += [testname]
	else:
		print "ok"

def unittests():
	global failed_tests
	class Test:
		def __init__(self):
			self.line = ""
			self.catch = None
			self.find = None
			self.err = 0 # expect 0 per default
	
	tests = []
	state = 0
	for line in file('src/base.lua'):
		if state == 0:
			if "@UNITTESTS" in line:
				state = 1
		else:
			if "@END" in line:
				state = 0
			else:
				test = Test()
				(args, cmdline) = line.split(":", 1)
				test.line = cmdline.strip()
				args = args.split(";")
				for arg in args:
					arg,value = arg.split("=")
					arg = arg.strip()
					value = value.strip()
					if arg.lower() == "err":
						test.err = int(value)
					elif arg.lower() == "catch":
						test.catch = value[1:-1]
					elif arg.lower() == "find":
						test.find = value[1:-1]
				tests += [test]
	
	olddir = os.getcwd()
	os.chdir(output_path+"/unit")
	
	for test in tests:
		f = file("bam.lua", "w")
		if test.catch != None:
			print >>f, "print(\"CATCH:\", %s)"%(test.line)
		else:
			print >>f, test.line
		print >>f, 'DefaultTarget(PseudoTarget("Test"))'
		f.close()

		print  "%s:"%(test.line),
		p = subprocess.Popen(bam + " --dry", stdout=subprocess.PIPE, shell=True, stderr=subprocess.STDOUT)
		report = p.stdout.readlines()
		p.wait()
		ret = p.returncode
		
		failed = False
		if ret != test.err:
			failed = True
			print "FAILED! error %d != %d" % (test.err, ret)
		
		if test.catch != None:
			found = False
			for l in report:
				l = l.split("CATCH:", 1)
				if len(l) == 2:
					catched = l[1].strip()
					if catched == test.catch:
						found = True
					else:
						print "FAILED! catch '%s' != '%s'" % (test.catch, catched)
						
			if not found:
				failed = True
		
		if test.find != None:
			found = False
			for l in report:
				if test.find in l:
					found = True
			
			if not found:
				failed = True
				print "FAILED! could not find '%s' in output" % (test.find)
		if failed or verbose:
			if failed:
				failed_tests += [test.line]
			else:
				print "",
			for l in report:
				print "\t", l.rstrip()
		else:
			print "ok"
			

	os.chdir(olddir)

# clean
shutil.rmtree(output_path, True)
# copy tree
copytree("tests", output_path)
os.mkdir(os.path.join(output_path, "unit"))

# run smaller unit tests
if len(tests) == 0:
	unittests()

# run bigger test cases
test("cyclic")
difftest("cyclic", "--debug-nodes", "--debug-nodes -n")
test("include_paths")
difftest("include_paths", "--debug-nodes", "--debug-nodes -n")
test("dot.in.dir")
difftest("dot.in.dir", "--debug-nodes", "--debug-nodes -n")

test("retval", "", 1)
test("multi_target", "SHOULD_NOT_EXIST", 1)
test("multi_target", "CORRECT_ONE")
test("collect_wrong", "", 1)
test("locked", "", 1)
test("cxx_dep")
test("deps", "", 1)
test("collect_recurse")
test("sharedlib")
test("deadlock")
test("addorder")

if len(failed_tests):
	print "FAILED TESTS:"
	for t in failed_tests:
		print "\t"+t
	sys.exit(1)
else:
	print "ALL TESTS PASSED!"
	sys.exit(0)

