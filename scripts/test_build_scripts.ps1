#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') {
    throw 'Run this test with Windows PowerShell (powershell.exe).'
}

$repo = Split-Path $PSScriptRoot -Parent
$temp = Join-Path ([IO.Path]::GetTempPath()) ('bam batch tests ' + [guid]::NewGuid())
$scripts = @(
    'make_win64_msvc.bat',
    'make_win32_msvc.bat',
    'make_win32_mingw.bat',
    'make_win32_dmc.bat'
)
$toolSource = @'
using System;
using System.IO;
using System.Reflection;
public class FakeBuildTool {
    public static int Main(string[] args) {
        string stage = "build";
        if (Path.GetFileNameWithoutExtension(Assembly.GetExecutingAssembly().Location) == "txt2c")
            stage = "generate";
        else {
            foreach (string arg in args)
                if (arg.EndsWith("txt2c.c")) stage = "helper";
        }
        File.AppendAllText(Environment.GetEnvironmentVariable("TEST_TRACE"), stage + "\n");
        string flag = Environment.GetEnvironmentVariable("TEST_MODE_FLAG");
        int flagIndex = Array.IndexOf(args, flag);
        int linkIndex = Array.IndexOf(args, "/link");
        if ((stage == "build" && (flagIndex < 0 || (linkIndex >= 0 && flagIndex > linkIndex))) ||
            (stage != "build" && flagIndex >= 0)) {
            Console.Error.WriteLine("Incorrect mode flag forwarding in " + stage);
            return 14;
        }
        if (Environment.GetEnvironmentVariable("TEST_FAIL_STAGE") == stage) {
            Console.Error.WriteLine("Simulated " + stage + " failure");
            return stage == "helper" ? 11 : stage == "generate" ? 12 : 13;
        }
        if (stage == "helper")
            File.Copy(Assembly.GetExecutingAssembly().Location, @"src\tools\txt2c.exe", true);
        if (stage == "generate") Console.WriteLine("/* generated */");
        if (stage == "build") {
            File.WriteAllText("test.obj", "");
            File.WriteAllText("bam.exp", "");
        }
        return 0;
    }
}
'@

$passed = 0
try {
    New-Item -ItemType Directory -Path $temp | Out-Null
    $tool = Join-Path $temp 'tool.exe'
    Add-Type -TypeDefinition $toolSource -OutputAssembly $tool -OutputType ConsoleApplication
    foreach ($script in $scripts) {
        $cases = @('success', 'helper', 'generate', 'build', 'missing-tool', 'header-output')
        if ($script -like '*msvc*') {
            $cases += @('missing-vs', 'setup')
        }
        foreach ($case in $cases) {
            $root = Join-Path $temp (($script -replace '[\\.]', '_') + ' ' + $case)
            $bin = Join-Path $root 'bin'
            $caller = Join-Path $root 'caller with spaces'
            $target = Join-Path $root $script
            New-Item -ItemType Directory -Path $bin, $caller, (Join-Path $root 'src\tools'),
                (Split-Path $target -Parent) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $repo $script) -Destination $target
            Set-Content -LiteralPath (Join-Path $root 'src\tools\txt2c.c') -Value 'fixture'
            Set-Content -LiteralPath (Join-Path $root 'src\main.c') -Value 'fixture'
            if ($case -ne 'missing-tool') {
                foreach ($name in @('cl', 'gcc', 'dmc')) {
                    Copy-Item -LiteralPath $tool -Destination (Join-Path $bin "$name.exe")
                }
            }
            if ($case -eq 'header-output') {
                New-Item -ItemType Directory -Path (Join-Path $root 'src\internal_base.h') | Out-Null
            }

            $trace = Join-Path $root 'trace.txt'
            $modeFlag = if ($script -like '*msvc*') { '/DBAM_STAT_THREAD_MODE=2' } else { '-DBAM_STAT_THREAD_MODE=2' }
            $wrapper = Join-Path $caller 'run.bat'
            $commands = @(
                '@echo off',
                'set "original_dir=%CD%"',
                'set "build_result=sentinel"',
                'set "ERRORLEVEL="',
                "set `"PATH=$bin;$env:SystemRoot\System32`"",
                "set `"VCINSTALLDIR=$root`"",
                "set `"ProgramFiles(x86)=$root`"",
                "set `"TEST_TRACE=$trace`"",
                "set `"TEST_MODE_FLAG=$modeFlag`"",
                "set `"TEST_FAIL_STAGE=$case`""
            )
            foreach ($version in @(140, 120, 110, 100, 90, 80)) {
                $commands += "set `"VS${version}COMNTOOLS=`""
            }
            if ($case -in @('missing-vs', 'setup')) {
                $commands += 'set "VCINSTALLDIR="'
            }
            if ($case -eq 'setup') {
                $commonTools = Join-Path $root 'vs\Common7\Tools'
                $vc = Join-Path $root 'vs\vc'
                New-Item -ItemType Directory -Path $commonTools, $vc -Force | Out-Null
                Set-Content -LiteralPath (Join-Path $commonTools 'vsvars32.bat') -Value '@exit /b 7'
                Set-Content -LiteralPath (Join-Path $vc 'vcvarsall.bat') -Value '@exit /b 7'
                $commands += "set `"VS140COMNTOOLS=$commonTools\`""
            }
            $commands += @(
                "call `"$target`" $modeFlag",
                'set "result=%errorlevel%"',
                'if not "%CD%"=="%original_dir%" exit /b 99',
                'if not "%build_result%"=="sentinel" exit /b 98',
                'exit /b %result%'
            )
            Set-Content -LiteralPath $wrapper -Value $commands -Encoding ASCII
            $expectedCode = switch ($case) {
                'success' { 0 }
                'helper' { 11 }
                'generate' { 12 }
                'build' { 13 }
                'missing-tool' { 9009 }
                default { 1 }
            }
            $expectedTrace = switch ($case) {
                'success' { 'helper,generate,build' }
                'helper' { 'helper' }
                'generate' { 'helper,generate' }
                'build' { 'helper,generate,build' }
                'header-output' { 'helper' }
                default { '' }
            }
            $startInfo = New-Object Diagnostics.ProcessStartInfo
            $startInfo.FileName = $env:ComSpec
            $startInfo.Arguments = "/d /c call `"$wrapper`""
            $startInfo.WorkingDirectory = $caller
            $startInfo.UseShellExecute = $false
            $startInfo.RedirectStandardOutput = $true
            $startInfo.RedirectStandardError = $true
            $process = New-Object Diagnostics.Process
            $process.StartInfo = $startInfo
            try {
                if (-not $process.Start()) {
                    throw "Failed to start cmd.exe for $script / $case."
                }
                $stdout = $process.StandardOutput.ReadToEndAsync()
                $stderr = $process.StandardError.ReadToEndAsync()
                if (-not $process.WaitForExit(30000)) {
                    Stop-Process -Id $process.Id -Force
                    throw "$script / ${case}: timed out."
                }
                $output = $stdout.Result + $stderr.Result
                $code = $process.ExitCode
            } finally {
                $process.Dispose()
            }
            $actualTrace = ''
            if (Test-Path -LiteralPath $trace) {
                $actualTrace = (Get-Content -LiteralPath $trace) -join ','
            }
            if ($code -ne $expectedCode -or $actualTrace -ne $expectedTrace) {
                throw "$script / ${case}: exit $code (expected $expectedCode), trace '$actualTrace' (expected '$expectedTrace').`n$output"
            }
            if (-not (Test-Path -LiteralPath (Join-Path $root 'src\tools\txt2c.c'))) {
                throw "$script / ${case}: helper source was not restored."
            }
            if ($case -eq 'success' -and $script -notlike '*mingw*' -and
                (Test-Path -LiteralPath (Join-Path $root 'test.obj'))) {
                throw "$script / ${case}: object cleanup failed."
            }
            Write-Host "PASS $script / $case"
            $passed++
        }
    }
    Write-Host "$passed batch build regression tests passed."
} finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp -Recurse -Force
    }
}
