param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot "build/windows-msvc"
$outputDirectory = Join-Path $repositoryRoot "artifacts/audio/m9-4-gravity-references"

cmake --build $buildDirectory --target reverb_gravity_reference_cli --config $Configuration -j 4
if ($LASTEXITCODE -ne 0) { throw "Gravity reference generator build failed" }

$generator = Join-Path $buildDirectory "src/render/$Configuration/reverb_gravity_reference_cli.exe"
& $generator --output-dir $outputDirectory
if ($LASTEXITCODE -ne 0) { throw "Gravity reference generation failed" }

Write-Host "Gravity reference fixtures written to $outputDirectory"
