# Thin shim. Determine version, delegate to the shared publisher.
# See cameraunlock-core/powershell/NightlyRelease.psm1 for what it does.

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

# -NoNexusZip: this mod is installer-only. No mod manager can deploy a payload that has to
# sit beside the executable in Binaries2\Win64, so the packager builds no Nexus ZIP and the
# publisher's default of treating a missing one as fatal would fail every nightly.
Publish-NightlyBuild `
    -ModId 'thief' `
    -ModName 'ThiefHeadTracking' `
    -Version (Get-ModVersion -ProjectRoot $projectRoot) `
    -ProjectRoot $projectRoot `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
