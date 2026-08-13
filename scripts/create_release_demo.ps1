param(
    [string] $Output = 'artifacts/ui/m7-6-alpha-release/reverb-playground-alpha-demo.mp4'
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
        'artifacts/ui/m6-3-factory-patches/factory-patch-switching.mp4',
        'artifacts/ui/m5-5-topology-crossfade/live-topology-edit.mp4',
        'artifacts/ui/m5-2-lfo-control-mapping/live-control-preview.mp4',
        'artifacts/ui/m4-3-stereo-impulse-decay/stereo-response-measurement-and-navigation.mp4'
    )
    foreach ($input in $inputs) {
        if (-not (Test-Path -LiteralPath $input)) { throw "Missing demonstration source: $input" }
    }

    $filter = @"
[0:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v0];
[1:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v1];
[2:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v2];
[3:v]scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1[v3];
[v0][v1][v2][v3]concat=n=4:v=1:a=0,
drawbox=x=24:y=22:w=510:h=54:color=0x0d1115@0.92:t=fill:enable='between(t,0,10)+between(t,10,24)+between(t,24,32)+between(t,32,44)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='1  BARR REFERENCE':fontcolor=0x45d0cc:fontsize=27:x=42:y=34:enable='between(t,0,10)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='2  CONSTRUCT AND EDIT':fontcolor=0x45d0cc:fontsize=27:x=42:y=34:enable='between(t,10,24)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='3  MODULATE':fontcolor=0xbd9cff:fontsize=27:x=42:y=34:enable='between(t,24,32)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='4  CAPTURE AND INSPECT':fontcolor=0xf2b44e:fontsize=27:x=42:y=34:enable='between(t,32,44)',
format=yuv420p[out]
"@ -replace "`r?`n", ""

    & $ffmpeg -y `
        -i $inputs[0] -i $inputs[1] -i $inputs[2] -i $inputs[3] `
        -filter_complex $filter -map '[out]' -an `
        -c:v libx264 -preset medium -crf 20 -movflags +faststart $outputPath
    if ($LASTEXITCODE -ne 0) { throw 'Release demonstration rendering failed.' }
    Write-Host "Created $outputPath"
} finally {
    Pop-Location
}
