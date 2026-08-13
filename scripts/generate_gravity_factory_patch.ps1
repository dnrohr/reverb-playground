param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot "build/windows-msvc"
$outputPath = Join-Path $repositoryRoot "factory-patches/gravity-diffusion.rvp.json"

cmake --build $buildDirectory --target reverb_gravity_reference_cli --config $Configuration -j 4
if ($LASTEXITCODE -ne 0) { throw "Gravity factory generator build failed" }

$generator = Join-Path $buildDirectory "src/render/$Configuration/reverb_gravity_reference_cli.exe"
& $generator --export-factory-patch $outputPath
if ($LASTEXITCODE -ne 0) { throw "Gravity factory generation failed" }

Write-Host "Gravity factory patch written to $outputPath"
