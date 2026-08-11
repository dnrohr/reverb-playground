$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$source = Join-Path $PSScriptRoot 'VST3/Reverb Playground.vst3'
$destinationRoot = Join-Path $env:LOCALAPPDATA 'Programs/Common/VST3'
$destination = Join-Path $destinationRoot 'Reverb Playground.vst3'
if (-not (Test-Path -LiteralPath $source)) { throw "Package is missing $source" }
New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null
if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
Copy-Item -LiteralPath $source -Destination $destination -Recurse
Write-Host "Installed Reverb Playground VST3 for the current user at $destination"
