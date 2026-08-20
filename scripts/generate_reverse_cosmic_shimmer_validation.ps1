param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_reverse_cosmic_shimmer_validation_cli.exe"
$fixtures = Join-Path $root "artifacts/audio/m13-3-reverse-cosmic-shimmer"
$report = Join-Path $root "artifacts/measurements/reverse-cosmic-shimmer-v1.json"

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Build reverb_reverse_cosmic_shimmer_validation_cli ($Configuration) before generating the artifacts."
}

& $executable --output-directory $fixtures --report $report
if ($LASTEXITCODE -ne 0) { throw "Reverse Cosmic Shimmer validation failed." }
