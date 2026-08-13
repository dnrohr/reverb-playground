param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',
    [string] $OutputRoot = 'out/packages',
    [switch] $AllowDirty
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (-not $AllowDirty) {
        $changes = @(git status --porcelain --untracked-files=normal)
        if ($LASTEXITCODE -ne 0) { throw 'Unable to inspect the source worktree.' }
        if ($changes.Count -ne 0) { throw 'Refusing to package a modified or untracked working tree. Commit first or use -AllowDirty for local validation.' }
    }

    $commit = (git rev-parse --short=12 HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{12}$') { throw 'Unable to resolve the source commit.' }
    $epoch = [int64](git show -s --format=%ct HEAD).Trim()
    $projectText = Get-Content -Raw CMakeLists.txt
    if ($projectText -notmatch '(?s)project\(\s*ReverbPlayground\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Unable to resolve the project version.' }
    $version = $Matches[1]

    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    $cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
    if (-not $cmake -and (Test-Path -LiteralPath 'C:\Program Files\CMake\bin\cmake.exe')) { $cmake = 'C:\Program Files\CMake\bin\cmake.exe' }
    if (-not $cmake) { throw 'CMake was not found.' }
    $python = (Get-Command python -ErrorAction Stop).Source

    & $cmake --preset windows-msvc "-DREVERB_BUILD_COMMIT=$commit"
    if ($LASTEXITCODE -ne 0) { throw 'Release configure failed.' }
    $preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }
    & $cmake --build --preset $preset --target ReverbPlayground_Standalone ReverbPlayground_VST3 --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "$Configuration package build failed." }

    $packageName = "ReverbPlayground-$version-windows-x64"
    $outputDirectory = [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputRoot))
    $stage = Join-Path $outputDirectory $packageName
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Join-Path $stage 'Standalone'), (Join-Path $stage 'VST3') | Out-Null

    $artifactRoot = Join-Path $projectRoot "build/windows-msvc/src/app/ReverbPlayground_artefacts/$Configuration"
    Copy-Item -LiteralPath (Join-Path $artifactRoot 'Standalone/Reverb Playground.exe') -Destination (Join-Path $stage 'Standalone/Reverb Playground.exe')
    Copy-Item -LiteralPath (Join-Path $artifactRoot 'VST3/Reverb Playground.vst3') -Destination (Join-Path $stage 'VST3/Reverb Playground.vst3') -Recurse
    Copy-Item -LiteralPath LICENSE, THIRD_PARTY_NOTICES.md, ASSET_PROVENANCE.md -Destination $stage
    Copy-Item -LiteralPath docs/windows-package-installation.md -Destination (Join-Path $stage 'README.md')
    Copy-Item -LiteralPath scripts/install_vst3.ps1 -Destination (Join-Path $stage 'install-vst3.ps1')

    $buildInfo = [ordered]@{
        product = 'Reverb Playground'
        version = $version
        commit = $commit
        platform = 'windows-x64'
        configuration = $Configuration
        sourceDateEpoch = $epoch
        formats = @('Standalone', 'VST3')
    }
    $buildInfoJson = ($buildInfo | ConvertTo-Json) + "`n"
    [IO.File]::WriteAllText(
        (Join-Path $stage 'build-info.json'),
        $buildInfoJson,
        (New-Object Text.UTF8Encoding($false)))

    $archive = Join-Path $outputDirectory "$packageName.zip"
    & $python scripts/create_release_archive.py --source $stage --output $archive --epoch $epoch
    if ($LASTEXITCODE -ne 0) { throw 'Release archive creation failed.' }
    & $python scripts/validate_windows_package.py $archive --commit (git rev-parse HEAD)
    if ($LASTEXITCODE -ne 0) { throw 'Release archive identity or checksum validation failed.' }
    Write-Host "Packaged version $version commit $commit at $archive"
} finally {
    Pop-Location
}
