<#
.SYNOPSIS
    Copies the Ascent Combat Framework's author-written Claude skills into .claude/skills
    so Claude Code sessions can see them.

.DESCRIPTION
    ACF Ultimate ships ~43 SKILL.md packs under
        Plugins/Marketplace/AscentCombatFramework/Resources/Skills/
    written by the plugin author for Claude Code. Claude only discovers skills under
    .claude/skills/, so on a stock install those packs are invisible to every session -
    which is exactly what happened here until 2026-08-13 (ticket #150). Agents were
    reading ACF headers to answer questions the author had already written down.

    Plugins/Marketplace/ is gitignored (.gitignore:60, "reinstallable from Fab"), so the
    plugin is not in the repo and neither are these copies. This script is the committed
    artifact; its output is not. Re-run it after any ACF update to refresh the copies.

    Idempotent and safe: it records what it copied in a manifest and only ever removes
    directories that manifest names. Hand-written project skills sitting alongside are
    never touched.

.PARAMETER Clean
    Remove the previously-registered ACF skills and exit without re-copying.

.PARAMETER Quiet
    Suppress the per-skill lines; print only the summary.

.EXAMPLE
    .\Tools\Register-ACFSkills.ps1
    .\Tools\Register-ACFSkills.ps1 -Clean

.NOTES
    ASCII-only on purpose. A .ps1 saved as UTF-8 without a BOM is read as ANSI by
    PowerShell 5.1, and one non-ASCII character breaks the parse with a misleading
    error (see GoblinSiege 5.8/CLAUDE.md, PowerShell rule 8).
#>

[CmdletBinding()]
param(
    [switch] $Clean,
    [switch] $Quiet
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Paths. The script lives in <project>/Tools, so the project root is its parent.
# ---------------------------------------------------------------------------
$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# ACF can live in EITHER of two places and the answer changed on 2026-08-20 (#205): a 4.4.2
# install from Fab landed in the ENGINE while the project already carried its own 4.4 copy, two
# plugins claimed the name 'AscentCombatFramework', and UnrealBuildTool refused to build at all -
# "does not contain the AscentCombatFramework module, but lists it". The project copy was retired
# and the engine copy is now authoritative.
#
# So this script DISCOVERS the plugin instead of hardcoding one path. The engine install folder is
# named with an install-specific hash (ACFUAsce5ab7c1439afbV5), which is exactly the kind of string
# that must never be pasted into a script - it changes on reinstall. Project-local is searched
# first, because if that copy ever exists again it is the one a build would use.
$PluginRoot = $null
$searchRoots = @(
    (Join-Path $ProjectRoot 'Plugins\Marketplace'),
    'D:\Epic Games\UE_5.8\Engine\Plugins\Marketplace'
)
foreach ($root in $searchRoots) {
    if (-not (Test-Path $root)) { continue }
    $found = Get-ChildItem -Path $root -Recurse -Depth 2 -Filter 'AscentCombatFramework.uplugin' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $PluginRoot = $found.DirectoryName; break }
}
if (-not $PluginRoot) {
    # Keeps the path in the error message below meaningful rather than blank.
    $PluginRoot = Join-Path $ProjectRoot 'Plugins\Marketplace\AscentCombatFramework'
}
$SourceRoot  = Join-Path $PluginRoot  'Resources\Skills'
$SkillsRoot  = Join-Path $ProjectRoot '.claude\skills'
$Manifest    = Join-Path $SkillsRoot  '.acf-manifest.json'

function Write-Step {
    param([string] $Message)
    if (-not $Quiet) { Write-Host $Message }
}

# ---------------------------------------------------------------------------
# Remove whatever a previous run installed - and ONLY that.
# ---------------------------------------------------------------------------
$removed = 0
if (Test-Path $Manifest) {
    $previous = Get-Content $Manifest -Raw | ConvertFrom-Json
    foreach ($name in @($previous.skills)) {
        $dir = Join-Path $SkillsRoot $name
        if (Test-Path $dir) {
            Remove-Item $dir -Recurse -Force
            $removed++
        }
    }
    Remove-Item $Manifest -Force
    Write-Step "Removed $removed previously registered ACF skill(s)."
}

if ($Clean) {
    Write-Host "Clean complete. Removed $removed ACF skill(s) from $SkillsRoot"
    exit 0
}

# ---------------------------------------------------------------------------
# The plugin must actually be installed. It is gitignored, so on a fresh clone
# it will not be here and that is not a bug - it is a reinstall from Fab.
# ---------------------------------------------------------------------------
if (-not (Test-Path $SourceRoot)) {
    Write-Error @"
ACF skills not found at:
  $SourceRoot

Searched, in order:
  <project>/Plugins/Marketplace   then   the engine's Plugins/Marketplace

The plugin is gitignored (Plugins/Marketplace/) and an engine install lives outside the
repo entirely, so a fresh clone will have neither. Install it from Fab - into the ENGINE
is what this project expects since #205 - and re-run.

Do NOT end up with both: two plugins claiming the same name stop UnrealBuildTool dead
before it compiles anything.

  Fab listing: https://www.fab.com/listings/cc258205-8fcf-41e5-9b48-6ec44e46d7eb
"@
    exit 1
}

# Plugin version, recorded in the manifest so a future session can tell at a glance
# whether the copies are stale relative to the installed plugin.
$pluginVersion = 'unknown'
$upluginPath = Join-Path $PluginRoot 'AscentCombatFramework.uplugin'
if (Test-Path $upluginPath) {
    $uplugin = Get-Content $upluginPath -Raw | ConvertFrom-Json
    if ($uplugin.VersionName) { $pluginVersion = $uplugin.VersionName }
}

# ---------------------------------------------------------------------------
# Copy. Every source directory that contains a SKILL.md becomes a project skill.
# Sub-documents (e.g. chooser-actions/*.md) come along with the directory copy.
# ---------------------------------------------------------------------------
if (-not (Test-Path $SkillsRoot)) {
    New-Item -ItemType Directory -Path $SkillsRoot -Force | Out-Null
}

$installed = New-Object System.Collections.Generic.List[string]
$skipped   = New-Object System.Collections.Generic.List[string]

foreach ($dir in Get-ChildItem -Path $SourceRoot -Directory | Sort-Object Name) {
    $skillFile = Join-Path $dir.FullName 'SKILL.md'
    if (-not (Test-Path $skillFile)) {
        $skipped.Add("$($dir.Name) (no SKILL.md)")
        continue
    }

    $target = Join-Path $SkillsRoot $dir.Name

    # Refuse to clobber a directory this script does not own. The manifest pass above
    # already removed everything from the last run, so anything still standing here is
    # somebody else's - a hand-written project skill with a colliding name.
    if (Test-Path $target) {
        $skipped.Add("$($dir.Name) (a non-ACF skill of that name already exists)")
        continue
    }

    Copy-Item -Path $dir.FullName -Destination $target -Recurse -Force
    $installed.Add($dir.Name)
    Write-Step "  + $($dir.Name)"
}

# ---------------------------------------------------------------------------
# Manifest. This is what makes the next run safe.
# ---------------------------------------------------------------------------
$manifestObject = [ordered]@{
    generatedBy   = 'Tools/Register-ACFSkills.ps1'
    generatedUtc  = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    pluginVersion = $pluginVersion
    sourceRoot    = $SourceRoot
    note          = 'Generated - do not edit these skill copies. Edit the plugin source or re-run the script.'
    skills        = @($installed)
}
$manifestObject | ConvertTo-Json -Depth 4 | Set-Content -Path $Manifest -Encoding utf8

# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "Registered $($installed.Count) ACF skill(s) from plugin v$pluginVersion"
Write-Host "  into: $SkillsRoot"
if ($skipped.Count -gt 0) {
    Write-Host ""
    Write-Host "Skipped $($skipped.Count):"
    foreach ($s in $skipped) { Write-Host "  - $s" }
}
Write-Host ""
Write-Host "Start a NEW Claude Code session for these to appear in its skill list."
