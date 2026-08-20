<#
.SYNOPSIS
    Run the Goblin Siege adversarial QA agent and print where the report landed.

.DESCRIPTION
    Launches the editor with Tools/QA/gs_qa_agent.py handed to the editor's python
    entry point. The script opens each map, starts PIE, drives the pawn adversarially
    for the configured time, writes Saved/QAReports/qa_run_<stamp>.json and .csv, and
    quits the editor by itself.

    Nothing here needs a compile: the agent is editor Python and touches no C++.

.EXAMPLE
    .\Tools\QA\Run-QA.ps1
    .\Tools\QA\Run-QA.ps1 -Seed 1337 -Seconds 300
    .\Tools\QA\Run-QA.ps1 -Maps "/Game/Maps/Test/L_CombatArena" -Seconds 90
#>
[CmdletBinding()]
param(
    [int]    $Seed = 20260819,
    [int]    $Seconds = 180,
    [string] $Maps = "/Game/Maps/L_Tutorial_Island,/Game/Maps/Test/L_CombatArena",
    [int]    $TimeoutMinutes = 45,
    [switch] $KeepEditorOpen
)

$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project    = Join-Path $ProjectDir 'MyProject.uproject'
$Script     = Join-Path $PSScriptRoot 'gs_qa_agent.py'
$ReportDir  = Join-Path $ProjectDir 'Saved\QAReports'

if (-not (Test-Path $Project)) { throw "cannot find the project at $Project" }
if (-not (Test-Path $Script))  { throw "cannot find the agent at $Script" }

# -- engine ------------------------------------------------------------------
# The registered install is the first lookup; the hard-coded path is the fallback
# this machine actually uses (D:\Epic Games\UE_5.8 - note the space).
$EngineRoot = $null
try {
    $builds = Get-ItemProperty -Path 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction Stop
    if ($builds.'5.8') { $EngineRoot = $builds.'5.8' }
} catch { }
if (-not $EngineRoot -or -not (Test-Path $EngineRoot)) { $EngineRoot = 'D:\Epic Games\UE_5.8' }
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $Editor)) { throw "cannot find UnrealEditor.exe under $EngineRoot" }

# -- refuse to fight a running editor ----------------------------------------
$running = @(Get-Process UnrealEditor -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    throw "an editor is already running (PID $($running[0].Id)). Close it first - the agent drives PIE and needs the project to itself."
}

# -- config passed to the python side ----------------------------------------
$env:GSQA_SEED    = $Seed
$env:GSQA_SECONDS = $Seconds
if ($KeepEditorOpen) { $env:GSQA_QUIT = '0' } else { $env:GSQA_QUIT = '1' }

Write-Host ''
Write-Host 'GOBLIN SIEGE - ADVERSARIAL QA AGENT' -ForegroundColor Cyan
Write-Host ("  seed        {0}" -f $Seed)
Write-Host ("  per map     {0}s" -f $Seconds)
Write-Host ("  maps        {0}" -f $Maps)
Write-Host ("  reports     {0}" -f $ReportDir)
Write-Host ''
Write-Host 'Launching the editor. It will open each map, play itself, and close.' -ForegroundColor DarkGray
Write-Host ''

$before = @()
if (Test-Path $ReportDir) {
    $before = @(Get-ChildItem $ReportDir -Filter 'qa_run_*.json' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
}

# -- stage the scripts somewhere without a space --------------------------------
# -ExecutePythonScript runs the script and then EXITS the editor, which kills the
# tick callback this tooling is built on before it sees a single frame. -ExecCmds
# runs after startup and leaves the editor alive, but its value cannot carry nested
# quotes, so the script path must contain no space - and "GoblinSiege 5.8" does.
# Copying the three files to a staging directory is the whole fix.
$Stage = Join-Path $env:TEMP 'gsqa'
if (-not (Test-Path $Stage)) { New-Item -ItemType Directory -Path $Stage | Out-Null }
Copy-Item (Join-Path $PSScriptRoot 'gs_qa_core.py')  $Stage -Force
Copy-Item (Join-Path $PSScriptRoot 'gs_qa_agent.py') $Stage -Force
Copy-Item (Join-Path $PSScriptRoot 'gs_qa_tests.py') $Stage -Force
$env:GSQA_DIR = $Stage
$Staged = Join-Path $Stage 'gs_qa_agent.py'

# ONE EDITOR PER MAP, on purpose. Tearing PIE down mid-run can take the editor with it
# (access violation in python311.dll during the reference collector's pass over
# Python-held UObjects), and the crash reporter then RELAUNCHES the editor, which
# re-arms the script, which opens a map, which starts PIE... A launch that plays one
# map and exits never performs a mid-run teardown. GSQA_SESSION stitches the launches
# back into one report; GSQA_TAG keeps them writing to one pair of files.
$Session = [Guid]::NewGuid().ToString('N').Substring(0, 8)
$Tag     = Get-Date -Format 'yyyyMMdd_HHmmss'
$env:GSQA_SESSION = $Session
$env:GSQA_TAG     = $Tag

$started = Get-Date
foreach ($map in ($Maps -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
    Write-Host ("--- $map") -ForegroundColor DarkGray
    $env:GSQA_MAPS = $map
    $argLine = '"{0}" -nosplash -stdout -FullStdOutLogOutput -nocrashreports -ExecCmds="py {1}"' -f $Project, $Staged
    $null = Start-Process -FilePath $Editor -ArgumentList $argLine -PassThru

    # Wait on the PROCESS NAME, not on the handle Start-Process returned: UnrealEditor.exe
    # hands off to a second process of the same name and the first exits within seconds,
    # so watching the handle reports "editor exited" while the real run is still going.
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    Start-Sleep -Seconds 20
    while (@(Get-Process UnrealEditor -ErrorAction SilentlyContinue).Count -gt 0) {
        if ((Get-Date) -gt $deadline) {
            Write-Warning "timed out after $TimeoutMinutes minutes on $map - killing the editor"
            Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
            break
        }
        Start-Sleep -Seconds 5
    }
    # A teardown crash leaves the reporter behind, and it relaunches the editor if left
    # alone. The report is already on disk by this point, so this is pure hygiene.
    Get-Process CrashReportClientEditor -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 3
    Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
}
Write-Host ("all maps done in {0:N1} minutes" -f ((Get-Date) - $started).TotalMinutes) -ForegroundColor DarkGray

# -- surface the report ------------------------------------------------------
if (-not (Test-Path $ReportDir)) {
    Write-Warning "no report directory at $ReportDir - the run produced nothing. Check Saved\Logs\MyProject.log for [GSQA] lines."
    exit 1
}
$newest = Get-ChildItem $ReportDir -Filter 'qa_run_2*.json' |
          Where-Object Name -notin $before |
          Sort-Object LastWriteTime -Descending |
          Select-Object -First 1
if (-not $newest) {
    Write-Warning "the run wrote no new report. Check Saved\Logs\MyProject.log for [GSQA] lines."
    exit 1
}

$report = Get-Content $newest.FullName -Raw | ConvertFrom-Json
Write-Host ''
Write-Host ('REPORT  ' + $newest.FullName) -ForegroundColor Green
Write-Host ('CSV     ' + ($newest.FullName -replace '\.json$', '.csv')) -ForegroundColor Green
Write-Host ''
Write-Host ('{0} distinct findings, {1} occurrences' -f $report.summary.total_findings, $report.summary.total_occurrences)
foreach ($sev in @('critical', 'high', 'medium', 'low')) {
    $n = $report.summary.by_severity.$sev
    if ($n) { Write-Host ('  {0,-9} {1}' -f $sev, $n) }
}
Write-Host ''
foreach ($f in ($report.findings | Select-Object -First 10)) {
    Write-Host ('  [{0}] {1} (x{2})' -f $f.severity, $f.error_type, $f.occurrences) -ForegroundColor Yellow
    Write-Host ('      {0}' -f $f.summary)
    Write-Host ('      {0} at {1:N0}, {2:N0}, {3:N0}' -f $f.location.map, $f.location.x, $f.location.y, $f.location.z) -ForegroundColor DarkGray
}
Write-Host ''
