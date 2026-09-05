param(
    [string] $Output = 'artifacts/ui/m33-usability-alpha-refresh/reverb-playground-alpha-2-demo.mp4'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    $ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
    $outputPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null

    $inputs = @(
        'artifacts/ui/m28-guidance-rendered-help/help-round-trip.mp4',
        'artifacts/ui/m29-hierarchical-compounds/matrix-hierarchy-workflow.mp4',
        'artifacts/ui/m30-interaction-state/interaction-state-workflow.mp4',
        'artifacts/ui/m31-predictable-layout/predictable-layout-workflow.mp4',
        'artifacts/ui/m32-crash-recovery/recovery-and-emergency-workflow.mp4'
    )
    foreach ($input in $inputs) {
        if (-not (Test-Path -LiteralPath $input)) { throw "Missing demonstration source: $input" }
    }

    $filter = @"
[0:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v0];
[1:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v1];
[2:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v2];
[3:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v3];
[4:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v4];
[v0][v1][v2][v3][v4]concat=n=5:v=1:a=0,
drawbox=x=24:y=22:w=650:h=54:color=0x0d1115@0.92:t=fill:enable='between(t,0,6.8)+between(t,6.8,19.4)+between(t,19.4,28.4)+between(t,28.4,36.4)+between(t,36.4,42.6)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='1  SEARCHABLE GUIDANCE':fontcolor=0x45d0cc:fontsize=27:x=42:y=34:enable='between(t,0,6.8)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='2  LIVE NESTED SCHEMATICS':fontcolor=0x45d0cc:fontsize=27:x=42:y=34:enable='between(t,6.8,19.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='3  STATE-CORRECT TUNING':fontcolor=0xbd9cff:fontsize=27:x=42:y=34:enable='between(t,19.4,28.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='4  PREDICTABLE LAYOUT':fontcolor=0xf2b44e:fontsize=27:x=42:y=34:enable='between(t,28.4,36.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='5  SAFE CRASH RECOVERY':fontcolor=0xff7c86:fontsize=27:x=42:y=34:enable='between(t,36.4,42.6)',
format=yuv420p[out]
"@ -replace "`r?`n", ""

    & $ffmpeg -y `
        -i $inputs[0] -i $inputs[1] -i $inputs[2] -i $inputs[3] -i $inputs[4] `
        -filter_complex $filter -map '[out]' -an `
        -c:v libx264 -preset medium -crf 20 -movflags +faststart $outputPath
    if ($LASTEXITCODE -ne 0) { throw 'Release demonstration rendering failed.' }
    Write-Host "Created $outputPath"
} finally {
    Pop-Location
}
