param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_split_feedback_shimmer_validation_cli.exe"
$output = Join-Path $root "artifacts/measurements/split-feedback-shimmer-v1.json"

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Build reverb_split_feedback_shimmer_validation_cli ($Configuration) before generating the artifact."
}

& $executable --output $output
if ($LASTEXITCODE -ne 0) { throw "Split Feedback Shimmer validation failed." }
