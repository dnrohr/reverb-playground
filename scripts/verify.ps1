param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot

try {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        $fallback = 'C:\Program Files\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $fallback) {
            $cmakePath = $fallback
        } else {
            throw 'CMake was not found. Install CMake 3.25 or newer and add it to PATH.'
        }
    } else {
        $cmakePath = $cmake.Source
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) {
        throw 'Python 3 was not found. It is required for repository checks.'
    }

    $pnpm = Get-Command pnpm -ErrorAction SilentlyContinue
    if (-not $pnpm) {
        throw 'pnpm 11.16.0 was not found. Install pnpm and add it to PATH.'
    }

    & $pnpm.Source --dir web install --frozen-lockfile
    if ($LASTEXITCODE -ne 0) { throw 'Web dependency installation failed.' }
    & $pnpm.Source --dir web typecheck
    if ($LASTEXITCODE -ne 0) { throw 'Web type checking failed.' }
    & $pnpm.Source --dir web test
    if ($LASTEXITCODE -ne 0) { throw 'Web tests failed.' }
    & $pnpm.Source --dir web build
    if ($LASTEXITCODE -ne 0) { throw 'Web asset build failed.' }

    & $python.Source -m unittest discover -s scripts -p 'test_*.py'
    if ($LASTEXITCODE -ne 0) { throw 'Repository policy tests failed.' }
    & $python.Source scripts/check_repository.py
    if ($LASTEXITCODE -ne 0) { throw 'Repository checks failed.' }

    & $cmakePath --preset windows-msvc
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

    $preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }
    & $cmakePath --build --preset $preset --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "$Configuration build failed." }

    $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
    if ($Configuration -eq 'Debug') {
        & $ctestPath --preset windows-debug
    } else {
        & $ctestPath --test-dir build/windows-msvc -C Release --output-on-failure
    }
    if ($LASTEXITCODE -ne 0) { throw "$Configuration tests failed." }

    Write-Host "Verification passed ($Configuration)."
} finally {
    Pop-Location
}
