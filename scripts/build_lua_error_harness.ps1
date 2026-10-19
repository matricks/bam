#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$compiler = Get-Command cl.exe -ErrorAction Stop
$output = Join-Path $repo 'test_output\lua_error_harness'
$sources = @(Get-ChildItem (Join-Path $repo 'src\*.c') |
    Where-Object Name -ne 'main.c' | ForEach-Object FullName)
$sources += @(Get-ChildItem (Join-Path $repo 'src\lua\*.c') | ForEach-Object FullName)
$sources += Join-Path $repo 'tests\lua_errors\harness.c'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Push-Location $output
try {
    & $compiler.Source /nologo /O2 /TC /D_CRT_SECURE_NO_DEPRECATE `
        /I (Join-Path $repo 'src') /I (Join-Path $repo 'src\lua') `
        @sources /Feharness.exe /link Advapi32.lib
    if ($LASTEXITCODE -ne 0) {
        throw "Lua error harness compilation failed (exit $LASTEXITCODE)."
    }
} finally {
    Pop-Location
}
Write-Output "Set BAM_LUA_ERROR_HARNESS to $(Join-Path $output 'harness.exe')."
