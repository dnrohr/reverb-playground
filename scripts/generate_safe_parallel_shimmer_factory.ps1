param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot "build/windows-msvc"
$outputPath = Join-Path $repositoryRoot "factory-patches/safe-parallel-shimmer.rvp.json"

cmake --build $buildDirectory --target reverb_factory_patch_cli --config $Configuration -j 4
if ($LASTEXITCODE -ne 0) { throw "Factory patch generator build failed" }

$generator = Join-Path $buildDirectory "src/render/$Configuration/reverb_factory_patch_cli.exe"
& $generator --export safe-parallel-shimmer $outputPath
if ($LASTEXITCODE -ne 0) { throw "Safe Parallel Shimmer factory generation failed" }

Write-Host "Safe Parallel Shimmer factory patch written to $outputPath"
