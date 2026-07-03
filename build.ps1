param(
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { "C:\Program Files\CMake\bin\cmake.exe" }

if (-not (Test-Path -LiteralPath $cmake)) {
    Write-Error "cmake nao foi encontrado no PATH nem em '$cmake'. Instale CMake ou rode este script pelo 'Developer PowerShell for VS 2022'."
}

$preset = if ($Configuration -eq "Release") { "msvc-release" } else { "msvc-debug" }
$buildPreset = if ($Configuration -eq "Release") { "release" } else { "debug" }

& $cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build --preset $buildPreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dll = Join-Path $PSScriptRoot "build\$preset\bin\$Configuration\pact_supply_tracker.dll"
Write-Host ""
Write-Host "Build finalizado:"
Write-Host $dll
