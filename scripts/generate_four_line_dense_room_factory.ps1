param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root "build/windows-msvc/src/render/$Configuration/reverb_factory_patch_cli.exe"
$output = Join-Path $root 'factory-patches/four-line-dense-room.rvp.json'
if (-not (Test-Path -LiteralPath $executable)) { throw "Factory generator is missing: $executable" }
& $executable --export four-line-fdn $output
if ($LASTEXITCODE -ne 0) { throw 'Four-Line Dense Room factory generation failed.' }
Write-Output $output
