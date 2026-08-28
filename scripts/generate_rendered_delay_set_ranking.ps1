param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_delay_set_response_cli.exe"
$output = Join-Path $root "artifacts/measurements/m22-rendered-delay-sets"
if (-not (Test-Path -LiteralPath $cli)) {
    throw "Missing $cli. Build reverb_delay_set_response_cli first."
}
& $cli --output-directory $output
if ($LASTEXITCODE -ne 0) { throw "Rendered delay-set ranking failed." }
