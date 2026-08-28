param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_delay_set_tuning_cli.exe"
$output = Join-Path $root "artifacts/measurements/m22-delay-set-candidates.json"
if (-not (Test-Path -LiteralPath $cli)) {
    throw "Missing $cli. Build reverb_delay_set_tuning_cli first."
}
& $cli --output $output
if ($LASTEXITCODE -ne 0) { throw "Delay-set candidate generation failed." }
