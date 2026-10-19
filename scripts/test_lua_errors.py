#!/usr/bin/env python
"""Exact-output regression tests for Bam's host-side Lua diagnostics."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
BAM = ROOT / ("bam.exe" if os.name == "nt" else "bam")


class LuaErrorTests(unittest.TestCase):
    def setUp(self):
        output = ROOT / "test_output"
        output.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="lua_errors_", dir=str(output))
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def write(self, name, source):
        path = self.directory / name
        path.write_text(source, encoding="utf-8")
        return path

    def invoke(self, source, flags=(), name="failure.lua", mode="-e", msvc=True):
        self.write(name, source)
        options = ["--win-msvc-mode"] if msvc else []
        environment = os.environ.copy()
        environment.pop("BAM_OPTIONS", None)
        result = subprocess.run(
            [str(BAM), mode, name] + options + list(flags), cwd=str(self.directory),
            env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=30,
        )
        return result.returncode, result.stdout

    def diagnostic(self, source, expected, **kwargs):
        status, output = self.invoke(source, **kwargs)
        self.assertEqual(status, 1, output)
        self.assertEqual(output.splitlines()[0], expected, output)
        self.assertEqual(output.count(": error:"), int(": error:" in expected), output)
        return output

    def test_default_colon_locations(self):
        for mode in ["-e", "-s"]:
            self.diagnostic('error("bad setting")', "failure.lua:1: error: bad setting",
                            mode=mode, msvc=False)
            self.diagnostic('assert(false, "assertion")', "failure.lua:1: error: assertion",
                            mode=mode, msvc=False)
            self.diagnostic("bam_sleep()", "failure.lua:1: error: sleep: expected 1 arguments",
                            mode=mode, msvc=False)
            status, output = self.invoke("local value = nil\nvalue()", mode=mode, msvc=False)
            self.assertEqual(status, 1, output)
            self.assertTrue(output.startswith("failure.lua:2: error: attempt to call"), output)
            status, output = self.invoke("local =", mode=mode, msvc=False)
            self.assertEqual(status, 1, output)
            self.assertTrue(output.startswith("failure.lua:1: error:"), output)
        self.write("imported.lua", 'error("import failure")')
        self.diagnostic('Import("imported.lua")', "imported.lua:1: error: import failure",
                        mode="-s", msvc=False)
        self.diagnostic('error([==[C:\\dir with spaces\\file.lua:123: bad: detail]==], 0)',
                        "C:\\dir with spaces\\file.lua:123: error: bad: detail", msvc=False)
        self.diagnostic('error("file.lua(12): error: already formatted", 0)',
                        "file.lua(12): error: already formatted", msvc=False)
        self.diagnostic('error("no location", 0)', "error: no location", msvc=False)

    def test_runtime_errors(self):
        self.diagnostic('error("bad setting")', "failure.lua(1): error: bad setting")
        self.diagnostic('assert(false, "assertion")', "failure.lua(1): error: assertion")
        self.diagnostic("bam_sleep()", "failure.lua(1): error: sleep: expected 1 arguments")
        status, output = self.invoke("local value = nil\nvalue()")
        self.assertEqual(status, 1, output)
        self.assertTrue(output.startswith("failure.lua(2): error: attempt to call"), output)

    def test_build_and_statthread_modes(self):
        for flags in [(), ("--no-statthread",)]:
            self.diagnostic('error("build failure")',
                            "failure.lua(1): error: build failure", mode="-s", flags=flags)

    def test_locations_and_message_text(self):
        cases = [
            ("C:\\dir with spaces\\file.lua:123: bad: detail",
             "C:\\dir with spaces\\file.lua(123): error: bad: detail"),
            ('[string "name:12: text"]:9: bad',
             '[string "name:12: text"](9): error: bad'),
            ("file.lua:99999999999999999999999999: bad",
             "file.lua(99999999999999999999999999): error: bad"),
            ("file.lua(12): error: already formatted",
             "file.lua(12): error: already formatted"),
            ("error: already categorized", "error: already categorized"),
            ("error: text:12: keep message intact", "error: text:12: keep message intact"),
            ("plain text: not a location", "error: plain text: not a location"),
            ("bad: embedded error: text", "error: bad: embedded error: text"),
        ]
        for message, expected in cases:
            with self.subTest(message=message):
                # Long brackets preserve backslashes without Lua escape processing.
                self.diagnostic("error([==[" + message + "]==], 0)", expected)
        path = self.write("file with spaces.lua", 'error("absolute path")')
        result = subprocess.run([str(BAM), "--win-msvc-mode", "-e", str(path)],
                                cwd=str(self.directory),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 1, result.stdout)
        # Lua may shorten a long source path; the drive colon must not break parsing.
        self.assertIn("file with spaces.lua(1): error: absolute path", result.stdout)

    def test_location_free_and_non_string_errors(self):
        for source, expected in [
            ('error("no location", 0)', "error: no location"),
            ("error(42)", "error: 42"),
            ("error(9223372036854775807)", "error: 9223372036854775807"),
            ("error(nil)", "error: Lua raised a nil value"),
            ("error(false)", "error: Lua raised a boolean value"),
            ("error({})", "error: Lua raised a table value"),
            ('error(setmetatable({}, {__tostring=function() error("must not run") end}))',
             "error: Lua raised a table value"),
        ]:
            with self.subTest(source=source):
                self.diagnostic(source, expected)

    def test_syntax_load_and_import_errors(self):
        for mode in ["-e", "-s"]:
            status, output = self.invoke("local =", mode=mode)
            self.assertEqual(status, 1, output)
            self.assertTrue(output.startswith("failure.lua(1): error:"), output)
            missing = subprocess.run([str(BAM), mode, "missing.lua"],
                                     cwd=str(self.directory), stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, timeout=30)
            self.assertEqual(missing.returncode, 1, missing.stdout)
            self.assertTrue(missing.stdout.startswith("error: cannot open missing.lua:"),
                            missing.stdout)
        self.write("imported.lua", "local =")
        status, output = self.invoke('Import("imported.lua")')
        self.assertEqual(status, 1, output)
        self.assertTrue(output.startswith("imported.lua(1): error:"), output)
        self.write("imported.lua", 'error("import failure")')
        self.diagnostic('Import("imported.lua")', "imported.lua(1): error: import failure",
                        mode="-s")

    def test_backtrace_locals_and_color(self):
        output = self.diagnostic('local detail = "local value"\nerror("trace")',
                                 "failure.lua(2): error: trace", flags=("-t", "-l"))
        self.assertIn("backtrace:", output)
        self.assertIn("detail = 'local value'", output)
        status, colored = self.invoke('error("color")', flags=("-r", "c"))
        self.assertEqual(status, 1, colored)
        self.assertEqual(colored.splitlines()[0],
                         "\033[01;31mfailure.lua(1): error: color\033[00m")

    def test_coroutines(self):
        status, output = self.invoke('coroutine.wrap(function() error("coroutine") end)()')
        self.assertEqual(status, 1, output)
        self.assertTrue(output.startswith("failure.lua(1): error:"), output)
        self.assertIn("coroutine", output)

    def test_handled_errors_and_success(self):
        source = """
local object = {}
local ok, err = pcall(error, object)
assert(not ok and rawequal(err, object))
ok, err = pcall(error, "caught")
assert(not ok and err == "caught")
local number = 42
ok, err = xpcall(function() error(number) end, function(value) return value end)
assert(not ok and type(err) == "number" and err == number)
ok, err = pcall(function() error("located") end)
assert(not ok and err:match("failure.lua:%d+: located"))
local co = coroutine.create(function() error("handled coroutine") end)
ok, err = coroutine.resume(co)
assert(not ok and err:match("failure.lua:%d+: handled coroutine"))
local chunk, load_error = loadfile("missing.lua")
assert(chunk == nil and load_error:match("cannot open missing.lua"))
print("ordinary output: unchanged")
"""
        status, output = self.invoke(source)
        self.assertEqual(status, 0, output)
        self.assertEqual(output, "ordinary output: unchanged\n")
        status, output = self.invoke('DefaultTarget(PseudoTarget("success"))',
                                     mode="-s", flags=("--dry",))
        self.assertEqual(status, 0, output)
        self.assertNotIn("error:", output)

    @unittest.skipUnless(os.environ.get("BAM_LUA_ERROR_HARNESS"),
                         "Set BAM_LUA_ERROR_HARNESS to the compiled C harness")
    def test_error_handler_and_memory_paths(self):
        result = subprocess.run([os.environ["BAM_LUA_ERROR_HARNESS"]],
                                cwd=str(self.directory),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("error: not enough memory", result.stdout)
        self.assertIn("error: error in error handling", result.stdout)
        self.assertIn("harness passed", result.stdout)
        for flags, location in [
            (["--panic"], "panic.lua:7"),
            (["--panic", "--win-msvc-mode"], "panic.lua(7)"),
        ]:
            panic = subprocess.run([os.environ["BAM_LUA_ERROR_HARNESS"]] + flags,
                                   cwd=str(self.directory),
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, timeout=30)
            self.assertNotEqual(panic.returncode, 0, panic.stdout)
            self.assertTrue(panic.stdout.startswith(location + ": error: unprotected failure\n"),
                            panic.stdout)
            self.assertIn("PANIC!", panic.stdout)

    @unittest.skipUnless(os.environ.get("BAM_MSBUILD"),
                         "Set BAM_MSBUILD to an installed MSBuild executable")
    def test_msbuild_recognition(self):
        self.write("failure.lua", 'error("msbuild failure")')
        path = ROOT / "tests" / "lua_errors" / "msbuild.proj"
        result = subprocess.run([os.environ["BAM_MSBUILD"], str(path), "/nologo", "/v:minimal",
                                 "/p:BamExe=" + str(BAM),
                                 "/p:ScriptDirectory=" + str(self.directory)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("Verified Lua error file=failure.lua line=1", result.stdout)


def run():
    return unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(LuaErrorTests)).wasSuccessful()


if __name__ == "__main__":
    raise SystemExit(0 if run() else 1)
