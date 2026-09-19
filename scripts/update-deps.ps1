#!/usr/bin/env pwsh
#Requires -Version 5.1
# ============================================================================
# <mod>/scripts/update-deps.ps1
# ============================================================================
# Bumps the vendored mod-loader copy under vendor/ultimate-asi-loader/ to the
# latest upstream release within the pinned version range, and writes refreshed
# LICENSE + README.md sidecar metadata.
#
# Usage:    pixi run update-deps
# Frequency: manual. The vendored copy is the install-time source of truth, so
# the dev runs this when they want a fresh upstream bump, reviews the diff, and
# commits the updated vendor/ tree. No build task depends on this, and CI never
# refreshes - it consumes whatever is committed under vendor/.
# ============================================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$projectRoot = Split-Path -Parent $PSScriptRoot

# The submodule, and only the submodule: a sibling checkout sits at whatever
# commit its own working tree is on, and vendoring from it would put a loader
# in the ZIP that nothing in this repo pins.
$modulePath = Join-Path $projectRoot 'cameraunlock-core/powershell/ModLoaderSetup.psm1'
if (-not (Test-Path $modulePath)) {
    throw "ModLoaderSetup.psm1 not found. Run 'pixi run sync' to update the cameraunlock-core submodule."
}
Import-Module $modulePath -Force

# --- CALL BLOCK ----------------------------------------------------------
# The game runs in the 64-bit Shipping-ThiefGame.exe, so it needs Ultimate ASI Loader x64. That release asset
# (Ultimate-ASI-Loader_x64.zip) is a wrapper zip containing a single x64
# dinput8.dll. Update-VendoredLoader cannot unwrap it, so it lands as
# vendor/ultimate-asi-loader/dinput8.dll *as a zip*; we extract the real DLL
# below. install.cmd copies the bundled dinput8.dll into Binaries2\Win64 under that same
# name, because Shipping-ThiefGame.exe imports DINPUT8.dll directly in both its 32- and
# 64-bit builds. It imports no VERSION.dll and ships no SDL2 - do not rename the loader.
$vendorDir = Join-Path $projectRoot 'vendor/ultimate-asi-loader'
$saved     = Join-Path $vendorDir 'dinput8.dll'
$readme    = Join-Path $vendorDir 'README.md'

# The module's own idempotency check compares the on-disk artifact against the
# hash of the downloaded *zip*, and what sits on disk here is the unwrapped DLL,
# so that check can never fire for this mod. Without the snapshot below, every
# run rewrites README.md with a fresh Fetched-at and dirties the tree on a
# no-op refresh.
$priorDllHash     = if (Test-Path $saved)  { (Get-FileHash -Path $saved -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
$priorReadmeBytes = if (Test-Path $readme) { [System.IO.File]::ReadAllBytes($readme) } else { $null }

Update-VendoredLoader `
    -Name 'ultimate-asi-loader' `
    -OutputDir $vendorDir `
    -OutputFileName 'dinput8.dll' `
    -Owner 'ThirteenAG' -Repo 'Ultimate-ASI-Loader' `
    -VersionPrefix 'v9.7.' `
    -AssetPattern '^Ultimate-ASI-Loader_x64\.zip$' `
    -LicenseUrl 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/master/license' | Out-Null

# Unwrap: the saved dinput8.dll is the wrapper zip. Replace it with the x64 DLL
# it contains.
#
# One named entry, never Expand-Archive over the whole thing. This archive is
# third-party input off a GitHub release, and Expand-Archive does not validate
# entry paths - an entry called '..\..\something' is written outside the
# destination, which here is inside the repo. Matching on the entry's leaf Name
# and extracting to a path we chose leaves nothing for a crafted entry name to
# steer.
$bytes = [System.IO.File]::ReadAllBytes($saved)
if ($bytes.Length -ge 2 -and $bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B) {
    $zipCopy = Join-Path $vendorDir '_loader.zip'
    Copy-Item $saved $zipCopy -Force
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [System.IO.Compression.ZipFile]::OpenRead($zipCopy)
        try {
            $entry = $zip.Entries | Where-Object { $_.Name -eq 'dinput8.dll' } | Select-Object -First 1
            if (-not $entry) { throw "x64 dinput8.dll not found inside Ultimate-ASI-Loader_x64.zip" }
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $saved, $true)
        } finally { $zip.Dispose() }
    } finally { Remove-Item $zipCopy -Force }
    Write-Host "Unwrapped x64 dinput8.dll from the release zip." -ForegroundColor Green
}

$dllHash = (Get-FileHash -Path $saved -Algorithm SHA256).Hash.ToLowerInvariant()

if ($priorReadmeBytes -and $priorDllHash -eq $dllHash) {
    # Byte for byte, so a run against an unchanged upstream leaves
    # `git status vendor/` clean instead of offering a timestamp-only diff.
    [System.IO.File]::WriteAllBytes($readme, $priorReadmeBytes)
    Write-Host ""
    Write-Host "Vendored loader unchanged (dinput8.dll sha256 $($dllHash.Substring(0,12))...). Nothing to commit." -ForegroundColor Green
    return
}

# The module's SHA-256 line covers the wrapper zip, which is not what we commit.
# Record the unwrapped DLL's hash too, so the committed artifact is verifiable.
$lines   = Get-Content $readme
$anchor  = ($lines | Select-String -SimpleMatch '- SHA-256:' | Select-Object -First 1).LineNumber
if (-not $anchor) { throw "vendor README.md has no '- SHA-256:' line to anchor the DLL hash after." }
# $anchor is 1-based, so it is also the index of the first line AFTER the
# anchor. Built as an explicit empty tail when there is none: PowerShell ranges
# count DOWN when the start is past the end, so `$lines[$n..($n-1)]` on an
# anchor that is the last line yields $null plus the last line over again, and
# the rewritten README ends with a duplicated line and a blank.
$tail = if ($anchor -lt $lines.Count) { @($lines[$anchor..($lines.Count - 1)]) } else { @() }
$lines = @($lines[0..($anchor - 1)]) + "- dinput8.dll SHA-256: ``$dllHash``" + $tail

# The module's closing line points at `pixi run package`, which is the older
# fetch-on-package shape. Refreshing the vendored copy is a manual dev action
# with a commit attached, so say so.
$lines = $lines | ForEach-Object {
    if ($_ -match '^Do not edit this directory by hand\.') {
        'Do not edit this directory by hand. Run ``pixi run update-deps`` to refresh, then commit.'
    } else { $_ }
}

# The statically linked components travel with the DLL rather than with the
# upstream repository, so their texts sit beside it and are listed here.
# package-release.ps1 hard-fails if either file is missing.
$lines += @(
    ''
    '## Bundled components'
    ''
    'The x64 target compiles these into `dinput8.dll`, so shipping that binary is a'
    'distribution of all of them, and each notice ships in the release ZIP:'
    ''
    '- miniz (MIT) - `miniz-LICENSE.txt` - https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/external/miniz/LICENSE'
    '- MinHook (BSD-2-Clause) - the loader compiles `external/injector/minhook`, and that'
    '  submodule points at TsudaKageyu/minhook, the same upstream project the mod compiles'
    '  into its own .asi. One text covers both copies, and the release ZIP ships it as'
    '  `licenses/minhook-LICENSE.txt` - https://github.com/TsudaKageyu/minhook/blob/master/LICENSE.txt'
    '- ThirteenAG/injector (zlib) - `external/injector/utility/FunctionHookMinHook` is the only'
    '  part of the injector tree itself that the x64 target compiles - `injector-LICENSE.txt` -'
    '  https://github.com/ThirteenAG/injector/blob/master/LICENSE'
    ''
    'MemoryModule, d3d8to9 and minidx9 are Win32-only targets and are absent from'
    'this binary. That set is a property of the release, not of the upstream'
    'repository - re-read premake5.lua on every bump.'
)
Set-Content -Path $readme -Value $lines -Encoding utf8

# --- END CALL BLOCK ------------------------------------------------------

Write-Host ""
Write-Host "Vendored dependencies refreshed. Review and commit the changes under vendor/." -ForegroundColor Green
Write-Host ""
# The set of components inside dinput8.dll is a property of the release being
# vendored, not of the upstream repository, and a bump can add one silently. At
# v9.7.4 the x64 target compiled MinHook (BSD-2-Clause), ThirteenAG/injector and miniz, and
# left MemoryModule (MPL-2.0), d3d8to9 and minidx9 to the 32-bit build. A bump
# that pulled MemoryModule into x64 would put an MPL component in the release
# ZIP with no source offer, which is the one licence in that set with an
# obligation the existing notices do not already discharge.
Write-Host "Before committing: re-read the Ultimate-ASI-Loader-x64 block in the new tag's" -ForegroundColor Yellow
Write-Host "premake5.lua. Every third-party source it compiles needs a section in" -ForegroundColor Yellow
Write-Host "THIRD-PARTY-NOTICES.md and its licence text in the release ZIP. miniz and injector" -ForegroundColor Yellow
Write-Host "ship theirs beside the DLL and packaging hard-fails without them; MinHook is the" -ForegroundColor Yellow
Write-Host "exception, covered by the copy the mod already ships from cameraunlock-core." -ForegroundColor Yellow
Write-Host "Refresh miniz-LICENSE.txt and injector-LICENSE.txt from the new tag's pinned" -ForegroundColor Yellow
Write-Host "external/ sources at the same time - they are committed, not fetched." -ForegroundColor Yellow
