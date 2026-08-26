param(
    [int]$MeasuredBlocks = 200,
    [string]$BuildDirectory = "build/windows-msvc",
    [string]$Output = "artifacts/measurements/performance-matrix-v1.json"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuild = Join-Path $projectRoot $BuildDirectory
$resolvedOutput = Join-Path $projectRoot $Output

cmake -S $projectRoot -B $resolvedBuild
if ($LASTEXITCODE -ne 0) { throw "Performance matrix configure failed." }
cmake --build $resolvedBuild --config Release --target reverb_performance_matrix_cli -j 4
if ($LASTEXITCODE -ne 0) { throw "Performance matrix Release build failed." }

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutput) | Out-Null
$executable = Join-Path $resolvedBuild "src/render/Release/reverb_performance_matrix_cli.exe"
& $executable --output $resolvedOutput --blocks $MeasuredBlocks
if ($LASTEXITCODE -ne 0) { throw "Performance matrix measurement failed." }

Write-Host "Performance matrix written to $resolvedOutput"
