"""Queue assertions and threaded/synchronous integration equivalence."""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
BAM = ROOT / ("bam.exe" if os.name == "nt" else "bam")
MODES = {"locking": 0, "legacy": 1, "atomic": 2}


class StatThreadTests(unittest.TestCase):
    def setUp(self):
        output = ROOT / "test_output"
        output.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="stat_thread_", dir=output)
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.environment = os.environ.copy()
        self.environment.pop("BAM_OPTIONS", None)

    def invoke(self, *flags):
        return subprocess.run(
            [str(BAM), *flags], cwd=self.directory, env=self.environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30,
        )

    def test_mode_and_queue_harness(self):
        help_result = self.invoke("--help")
        self.assertEqual(help_result.returncode, 0, help_result.stdout)
        match = re.search(r"^stat thread mode: (locking|legacy|atomic)$",
                          help_result.stdout, re.MULTILINE)
        self.assertIsNotNone(match, help_result.stdout)
        mode = MODES[match.group(1)]
        sources = [ROOT / "src" / name for name in ("mem.c", "path.c", "support.c")]
        sources.append(ROOT / "tests" / "stat_thread" / "harness.c")
        executable = self.directory / ("harness.exe" if os.name == "nt" else "harness")
        if os.name == "nt":
            command = ["cl", "/nologo", "/O2", "/D_CRT_SECURE_NO_DEPRECATE",
                       f"/DBAM_STAT_THREAD_MODE={mode}", f"/I{ROOT / 'src' / 'lua'}",
                       *map(str, sources), f"/Fe{executable}", "/link", "Advapi32.lib"]
        else:
            command = ["gcc", "-O2", "-DLUA_USE_POSIX", f"-DBAM_STAT_THREAD_MODE={mode}",
                       "-I", str(ROOT / "src" / "lua"), *map(str, sources),
                       "-o", str(executable), "-lm", "-lpthread", "-ldl"]
        compiled = subprocess.run(
            command, cwd=self.directory, env=self.environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120,
        )
        self.assertEqual(compiled.returncode, 0, compiled.stdout)
        result = subprocess.run(
            [str(executable)], cwd=self.directory, env=self.environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn(f"stat harness: {match.group(1)} passed", result.stdout)

    def test_graph_equivalence(self):
        (self.directory / "input").write_text("input", encoding="utf-8")
        (self.directory / "bam.lua").write_text(
            'local root = PseudoTarget("all")\n'
            'for i = 1, 2048 do\n'
            '  local output = "output" .. i\n'
            '  AddJob(output, "generate", "echo output > " .. output)\n'
            '  AddDependency(output, "input")\n'
            '  SkipOutputVerification(output)\n'
            '  AddDependency(root, output)\n'
            '  if i % 512 == 0 then bam_sleep(0.002) end\n'
            'end\n'
            'DefaultTarget(root)\n', encoding="utf-8",
        )
        for cache_flags in [(), ("-n",)]:
            for _ in range(3):
                threaded = self.invoke("--debug-nodes", "--dry", *cache_flags)
                synchronous = self.invoke("--debug-nodes", "--dry", "--no-statthread",
                                          *cache_flags)
                self.assertEqual(threaded.returncode, 0, threaded.stdout)
                self.assertEqual(synchronous.returncode, 0, synchronous.stdout)
                self.assertEqual(threaded.stdout, synchronous.stdout)

    def test_invalid_mode(self):
        probe = self.directory / "mode_probe.c"
        probe.write_text('#include "support.h"\n', encoding="utf-8")
        for mode in (-1, 3):
            if os.name == "nt":
                command = ["cl", "/nologo", "/c", f"/DBAM_STAT_THREAD_MODE={mode}",
                           f"/I{ROOT / 'src'}", str(probe)]
            else:
                command = ["gcc", "-c", f"-DBAM_STAT_THREAD_MODE={mode}",
                           "-I", str(ROOT / "src"), str(probe)]
            result = subprocess.run(
                command, cwd=self.directory, env=self.environment,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30,
            )
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("invalid BAM_STAT_THREAD_MODE", result.stdout)

    def test_incremental_build(self):
        input_file = self.directory / "input"
        input_file.write_text("input", encoding="utf-8")
        (self.directory / "bam.lua").write_text(
            'AddJob("output", "generate", "echo output > output", "input")\n'
            'DefaultTarget("output")\n', encoding="utf-8",
        )
        for flags in [(), ("--no-statthread",)]:
            output = self.directory / "output"
            if output.exists():
                output.unlink()
            first = self.invoke(*flags)
            self.assertEqual(first.returncode, 0, first.stdout)
            self.assertTrue(output.exists())
            before = output.stat().st_mtime_ns
            second = self.invoke(*flags)
            self.assertEqual(second.returncode, 0, second.stdout)
            self.assertEqual(output.stat().st_mtime_ns, before)
            os.utime(output, (1000000000, 1000000000))
            third = self.invoke(*flags)
            self.assertEqual(third.returncode, 0, third.stdout)
            self.assertGreater(output.stat().st_mtime, 1000000000)

    def test_error_shutdown(self):
        for prefix in ["", 'for i = 1, 2048 do AddJob("node" .. i, "job", "echo job") end\n']:
            (self.directory / "bam.lua").write_text(
                prefix + 'error("stat shutdown")\n', encoding="utf-8",
            )
            for flags in [(), ("--no-statthread",)]:
                result = self.invoke(*flags)
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertIn("stat shutdown", result.stdout)


def run():
    return unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(StatThreadTests)
    ).wasSuccessful()


if __name__ == "__main__":
    raise SystemExit(0 if run() else 1)
