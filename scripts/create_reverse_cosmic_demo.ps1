param(
    [string] $Output = 'artifacts/ui/m13-3-reverse-cosmic-shimmer/reverse-cosmic-complete-demo.mp4'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
    $inputs = @(
        'artifacts/ui/m13-3-reverse-cosmic-shimmer/reverse-cosmic-workflow.mp4',
        'artifacts/ui/m4-3-stereo-impulse-decay/stereo-response-measurement-and-navigation.mp4',
        'artifacts/ui/m5-6-runaway-feedback/runaway-mute-edit-undo-recovery.mp4',
        'artifacts/ui/m2-4-patch-persistence/save-load-invalid.mp4'
    )
    foreach ($input in $inputs) {
        if (-not (Test-Path -LiteralPath $input)) { throw "Missing demonstration source: $input" }
    }
    $filter = @"
drawbox=x=22:y=18:w=660:h=52:color=0x0d1115@0.92:t=fill,
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='1  SELECT / MODULATE / CONTINUOUS EDIT':fontcolor=0xbd9cff:fontsize=23:x=38:y=31:enable='between(t,0,7.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='2  MEASURE / INSPECT RESPONSE':fontcolor=0x45d0cc:fontsize=23:x=38:y=31:enable='between(t,7.4,19.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='3  SAFETY LATCH / EDIT / RECOVER':fontcolor=0xec575c:fontsize=23:x=38:y=31:enable='between(t,19.4,31.4)',
drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='4  SAVE / RELOAD / REJECT INVALID':fontcolor=0xf2b44e:fontsize=23:x=38:y=31:enable='between(t,31.4,37.4)',
format=yuv420p[out]
"@ -replace "`r?`n", ""
    $outputPath = [IO.Path]::GetFullPath((Join-Path $root $Output))
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ("reverb-cosmic-demo-" + [Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($temporary) | Out-Null
    try {
        $normalized = @()
        for ($index = 0; $index -lt $inputs.Count; ++$index) {
            $clip = Join-Path $temporary "clip-$index.mp4"
            $videoFilter = if ($index -eq 2) {
                'crop=1600:900:0:0,scale=1280:720,fps=30,setsar=1,format=yuv420p'
            } else {
                'scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2:black,fps=30,setsar=1,format=yuv420p'
            }
            & $ffmpeg -loglevel error -y -i $inputs[$index] -an `
                -vf $videoFilter `
                -c:v libx264 -crf 20 $clip
            if ($LASTEXITCODE -ne 0) { throw "Failed to normalize demonstration clip $index." }
            $normalized += $clip
        }
        $list = Join-Path $temporary 'clips.txt'
        $listText = ($normalized | ForEach-Object { "file '$($_ -replace "'", "''")'" }) -join "`n"
        [IO.File]::WriteAllText($list, $listText + "`n", (New-Object Text.UTF8Encoding($false)))
        $joined = Join-Path $temporary 'joined.mp4'
        & $ffmpeg -loglevel error -y -f concat -safe 0 -i $list -c copy $joined
        if ($LASTEXITCODE -ne 0) { throw 'Failed to join normalized demonstration clips.' }
        & $ffmpeg -loglevel error -y -i $joined -vf $filter -an -c:v libx264 -crf 20 `
            -movflags +faststart $outputPath
    } finally {
        if ([IO.Directory]::Exists($temporary)) { [IO.Directory]::Delete($temporary, $true) }
    }
    if ($LASTEXITCODE -ne 0) { throw 'Reverse Cosmic Shimmer demonstration rendering failed.' }
} finally {
    Pop-Location
}
