#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Package Thief Head Tracking into its release ZIP.
.DESCRIPTION
    Produces, in release/:
      ThiefHeadTracking-v<version>-installer.zip  install.cmd + launcher-manifest.json
                                                  + plugins/ + vendor/ + shared/ + docs

    ONE ZIP, and no Nexus ZIP - do not add one back. This payload has to land beside
    Shipping-ThiefGame.exe in Binaries2\Win64, and a mod manager deploys into one fixed
    folder inside the game, which is not that one. The mod is installer-only, the README
    says so, and scripts/release-nightly.ps1 passes -NoNexusZip for the same reason.

    Requires Node.js on PATH for the two release validators at the end.

    Offline and side-effect free: consumes whatever is committed under vendor/
    and whatever cameraunlock-core commit is checked out. Refreshing either is
    `pixi run update-deps` / `pixi run sync`, both deliberate acts with a commit
    attached.

    Non-interactive: exits 0 on success, non-zero with a one-line diagnostic on
    any failure.
.NOTES
    Run via: pixi run package
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$projectRoot = Split-Path -Parent $PSScriptRoot
$scriptsDir = Join-Path $projectRoot 'scripts'
$releaseDir = Join-Path $projectRoot 'release'

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

$version = Get-ModVersion -ProjectRoot $projectRoot

Write-Host '=== Thief Head Tracking - Package Release ===' -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan
Write-Host ''

$asiPath = Join-Path $projectRoot 'bin\Release\ThiefHeadTracking.asi'
if (-not (Test-Path $asiPath)) { throw "Build output not found: $asiPath (run 'pixi run build-release')" }

$vendorDir = Join-Path $projectRoot 'vendor\ultimate-asi-loader'
$vendorLoaderDll = Join-Path $vendorDir 'dinput8.dll'
if (-not (Test-Path $vendorLoaderDll)) {
    throw "Vendored ASI loader missing: $vendorLoaderDll (run 'pixi run update-deps')"
}

# The vendored loader is the one binary in the release ZIPs this repo did not
# compile, and it ships into the game's exe directory where Windows loads it
# under its own name, because the exe imports DINPUT8 directly. update-deps.ps1 records its SHA-256 in the
# sidecar README precisely so the committed artifact is verifiable; until this
# check existed nothing ever read that line back, so a DLL swapped in the
# working tree - or a bad partial write during the unwrap - was packaged and
# published without a word. Offline: it compares what is on disk against what
# is committed beside it, and refuses to build a ZIP when they disagree.
$vendorReadme = Join-Path $vendorDir 'README.md'
if (-not (Test-Path $vendorReadme)) {
    throw "Vendored ASI loader metadata missing: $vendorReadme (run 'pixi run update-deps')"
}
$recordedHash = [regex]::Match(
    (Get-Content -LiteralPath $vendorReadme -Raw),
    '(?m)^-\s*dinput8\.dll SHA-256:\s*`?([0-9a-fA-F]{64})`?').Groups[1].Value
if (-not $recordedHash) {
    throw "vendor/ultimate-asi-loader/README.md records no 'dinput8.dll SHA-256' line, so the shipped loader cannot be verified. Run 'pixi run update-deps' and commit the result."
}
$actualHash = (Get-FileHash -LiteralPath $vendorLoaderDll -Algorithm SHA256).Hash
if ($actualHash -ne $recordedHash.ToUpperInvariant()) {
    throw "vendor/ultimate-asi-loader/dinput8.dll does not match the SHA-256 recorded beside it (recorded $($recordedHash.ToLowerInvariant()), found $($actualHash.ToLowerInvariant())). Refusing to package a loader nothing in the repo vouches for."
}
Write-Host "  vendored loader verified against its recorded SHA-256" -ForegroundColor Green

foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    if (-not (Test-Path (Join-Path $scriptsDir $s))) { throw "Required script not found: scripts/$s" }
}

# CMakeLists.txt is canonical - release.yml reads the tag's version out of the
# same declaration - and release.ps1 writes every derived copy in one pass; this
# is what makes that true rather than intended.
Assert-ModVersionsInSync -ProjectRoot $projectRoot -Version $version

# MinHook and the Hacker Disassembler Engine (both BSD-2-Clause) are compiled
# into the .asi out of cameraunlock-core's vendored copy, so their notice has to
# travel with every ZIP that carries the binary.
$minhookLicence = Join-Path $projectRoot 'cameraunlock-core\vendor\minhook\LICENSE.txt'
if (-not (Test-Path $minhookLicence)) {
    throw "MinHook LICENSE.txt not found at $minhookLicence. It is compiled into the .asi and its notice must ship with it."
}

# The vendored dinput8.dll is a static binary and is not one component. Ultimate
# ASI Loader's x64 premake target compiles miniz (MIT) and ThirteenAG/injector
# (zlib) - which carries its own MinHook - into it, so shipping that DLL is a
# binary distribution of all three and each notice has to travel with it. Their
# texts sit beside the DLL they are inside rather than in a repo-level folder,
# so a loader bump and its licence set move together. MemoryModule, d3d8to9 and
# minidx9 are 32-bit only and are absent from this binary; re-check that in
# premake5.lua on any bump, because the set is a property of the release, not of
# the upstream repository.
$bundledLicences = [ordered]@{
    'miniz-LICENSE.txt'    = 'miniz'
    'injector-LICENSE.txt' = 'injector'
}
foreach ($lf in $bundledLicences.Keys) {
    if (-not (Test-Path (Join-Path $vendorDir $lf))) {
        throw "vendor/ultimate-asi-loader/$lf not found. $($bundledLicences[$lf]) is compiled into the vendored dinput8.dll and its notice must ship with it."
    }
}

if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

function New-StagingDir {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (Test-Path $Path) { Remove-Item -Recurse -Force $Path }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
    return $Path
}

function Compress-Staging {
    param(
        [Parameter(Mandatory = $true)][string]$StagingDir,
        [Parameter(Mandatory = $true)][string]$ZipPath
    )
    if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
    Push-Location $StagingDir
    try { Compress-Archive -Path '.\*' -DestinationPath $ZipPath -Force } finally { Pop-Location }
    Remove-Item -Recurse -Force $StagingDir
    Write-Host ("  $ZipPath ({0:N1} KB)" -f ((Get-Item $ZipPath).Length / 1KB)) -ForegroundColor Green
}

# --- Installer ZIP (GitHub Releases / the launcher) ---
Write-Host '--- Installer ZIP ---' -ForegroundColor Yellow
$ghStaging = New-StagingDir (Join-Path $releaseDir 'staging-installer')

foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    Copy-Item (Join-Path $scriptsDir $s) -Destination $ghStaging -Force
    Write-Host "  $s" -ForegroundColor Green
}

$pluginsDir = Join-Path $ghStaging 'plugins'
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host '  plugins/ThiefHeadTracking.asi' -ForegroundColor Green

$ghVendorDir = Join-Path $ghStaging 'vendor\ultimate-asi-loader'
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
foreach ($vf in @('dinput8.dll', 'LICENSE', 'README.md') + @($bundledLicences.Keys)) {
    $src = Join-Path $vendorDir $vf
    if (-not (Test-Path $src)) { throw "Vendored ASI loader file missing: vendor/ultimate-asi-loader/$vf" }
    Copy-Item $src -Destination $ghVendorDir -Force
    Write-Host "  vendor/ultimate-asi-loader/$vf" -ForegroundColor Green
}

Copy-LicenceNotices -StagingDir $ghStaging -ProjectRoot $projectRoot -Additional @('README.md', 'CHANGELOG.md')
# Also under licenses/, not only under vendor/. The ZIP then answers "what is
# in this thing and under what terms" from one folder.
Copy-Item (Join-Path $vendorDir 'LICENSE') `
          -Destination (Join-Path $ghStaging 'licenses\ultimate-asi-loader-LICENSE.txt') -Force
Write-Host '  licenses/ultimate-asi-loader-LICENSE.txt' -ForegroundColor Green
Copy-Item $minhookLicence -Destination (Join-Path $ghStaging 'licenses\minhook-LICENSE.txt') -Force
Write-Host '  licenses/minhook-LICENSE.txt' -ForegroundColor Green
foreach ($lf in $bundledLicences.Keys) {
    Copy-Item (Join-Path $vendorDir $lf) -Destination (Join-Path $ghStaging "licenses\$lf") -Force
    Write-Host "  licenses/$lf" -ForegroundColor Green
}

# shared/ carries install-body-asi.cmd and find-game.ps1, which scripts/install.cmd
# is a thin wrapper over. Without it the installer cannot run.
Copy-SharedBundle -StagingDir $ghStaging -CoreRoot (Join-Path $projectRoot 'cameraunlock-core')

# The launcher's contract with this package, at the ZIP root. mod_info.version is stamped
# from the build so the shipped manifest cannot disagree with the .asi beside it.
$launcherManifestPath = Join-Path $projectRoot 'launcher-manifest.json'
if (-not (Test-Path $launcherManifestPath)) {
    throw "launcher-manifest.json not found at: $launcherManifestPath"
}
# -Encoding UTF8: Windows PowerShell 5.1 reads with the ANSI codepage by default, and the
# write below is explicitly UTF-8, so without this the round trip mangles any non-ASCII in
# the manifest - mod_info.name is what the launcher stamps into its receipt.
$manifestText = Get-Content $launcherManifestPath -Raw -Encoding UTF8
$manifestText = $manifestText -replace '("version":\s*")\d+\.\d+\.\d+(")', "`${1}$version`$2"
[System.IO.File]::WriteAllText((Join-Path $ghStaging 'launcher-manifest.json'), $manifestText,
                               (New-Object System.Text.UTF8Encoding $false))
Write-Host "  launcher-manifest.json (version $version)" -ForegroundColor Green

Compress-Staging -StagingDir $ghStaging -ZipPath (Join-Path $releaseDir "ThiefHeadTracking-v$version-installer.zip")

# A Nexus ZIP built before this stage was removed would otherwise sit in release/ for ever,
# looking current, with the loader misnamed and the tree flattened to the game root.
Get-ChildItem (Join-Path $releaseDir '*-nexus.zip') -ErrorAction SilentlyContinue |
    ForEach-Object {
        Remove-Item $_.FullName -Force
        Write-Host "  removed stale $($_.Name)" -ForegroundColor Yellow
    }

# No Nexus ZIP stage here, and do not add one back.
#
# A mod manager deploys into ONE fixed subtree of the game folder, and this payload has to
# land beside the executable in Binaries2\Win64, which is not that subtree for any manager
# this mod has been checked against. A manager that installed it anyway would put the files
# where Ultimate ASI Loader never looks, report success, and load nothing. This mod is
# installer-only, and the README says so.
#
# Before claiming a specific manager cannot do it, read that manager's own per-game
# extension rather than asserting it: for Vortex that is queryModPath and any
# registerModType in bundledPlugins\game-<id>\index.js.

# --- Validate ---
# Checks every launcher-manifest.json `files[].source` really exists inside the ZIP that
# was just built, and that the third-party notices cover what ships. Renaming a staged path
# without updating the manifest produces a ZIP that CI calls good and the launcher rejects
# at deploy time; nothing else in the chain catches it.
Write-Host ''
Write-Host '--- Validate ---' -ForegroundColor Yellow

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "node is required to validate the built package (cameraunlock-core/scripts/validate-manifest.mjs). Install Node.js and re-run; a release must not be published unvalidated."
}

foreach ($validator in @('validate-manifest.mjs', 'validate-notices.mjs')) {
    $validatorScript = Join-Path $projectRoot "cameraunlock-core/scripts/$validator"
    if (-not (Test-Path $validatorScript)) {
        throw "Validator not found: $validatorScript"
    }
    node $validatorScript
    if ($LASTEXITCODE -ne 0) {
        throw "$validator failed - the built package does not match what it declares."
    }
}

Write-Host ''
Write-Host '=== Package Complete ===' -ForegroundColor Magenta
