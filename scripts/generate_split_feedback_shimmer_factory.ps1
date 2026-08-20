param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_factory_patch_cli.exe"
$output = Join-Path $root "factory-patches/split-feedback-shimmer.rvp.json"

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Build reverb_factory_patch_cli ($Configuration) before generating the factory patch."
}

& $executable --export split-feedback-shimmer $output
if ($LASTEXITCODE -ne 0) { throw "Split Feedback Shimmer factory generation failed." }
