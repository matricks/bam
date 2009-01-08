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

	olddir = os.getcwd()
	os.chdir(output_path+"/"+name)
	cmdline = bam+" -v "+extra_bam_flags+" " + moreflags
	
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
		failed_tests += [name]
	else:
		print " ok"

def unittests():
	class Test:
		def __init__(self):
			self.line = ""
			self.catch = None
			self.find = None
			self.err = None
	
	tests = []
	state = 0
	for line in file('src/base.bam'):
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
		f = file("default.bam", "w")
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
		if test.err != None and ret != test.err:
			failed = True
			print "FAILED! error %d != %d" % (test.err, ret)
		
		if test.catch != None:
			for l in report:
				l = l.split("CATCH:", 1)
				if len(l) == 2:
					catched = l[1].strip()
					if catched != test.catch:
						failed = True
						print "FAILED! catch '%s' != '%s'" % (test.catch, catched)
		
		if test.find != None:
			found = False
			for l in report:
				if test.find in l:
					found = True
			
			if not found:
				failed = True
				print "FAILED! could not find '%s' in output" % (test.find)
		if failed or verbose:
			if not failed:
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

# run smaller unit tests
unittests()

# run bigger test cases
test("cyclic")
test("include_paths")
test("dot.in.dir")
#test("subproject")
test("retval", "", 1)
test("multi_target", "SHOULD_NOT_EXIST", 1)
test("multi_target", "CORRECT_ONE")
test("collect_wrong", "", 1)
test("locked", "", 1)
test("collect_recurse")

if len(failed_tests):
	print "FAILED TESTS:"
	for t in failed_tests:
		print "\t"+t
else:
	print "ALL TESTS PASSED!"

