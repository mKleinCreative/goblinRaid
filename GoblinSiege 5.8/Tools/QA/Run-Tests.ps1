<#
.SYNOPSIS
    Run the Goblin Siege deterministic test suite and print the readable result.

.DESCRIPTION
    Launches the editor with Tools/QA/gs_qa_tests.py handed to the editor's python
    entry point. Every case runs against a live PIE session with fixed inputs and
    fixed expectations, so two runs of the same build give the same answer.

    Output lands in three places:
        Saved\QAReports\tests_latest.txt    the readable table (also printed here)
        Saved\QAReports\tests_<stamp>.txt   the same, kept
        Saved\QAReports\tests_<stamp>.json  per-check detail for tooling

    Exit code is 0 when nothing failed, 1 when something did - so this can gate a
    commit or a build without anyone reading it.

.EXAMPLE
    .\Tools\QA\Run-Tests.ps1
    .\Tools\QA\Run-Tests.ps1 -Maps "/Game/Maps/Test/L_CombatArena"
#>
[CmdletBinding()]
param(
    [string] $Maps = "/Game/Maps/Test/L_CombatArena,/Game/Maps/L_Tutorial_Island",
    [int]    $TimeoutMinutes = 30,
    [switch] $KeepEditorOpen
)

$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project    = Join-Path $ProjectDir 'MyProject.uproject'
$Script     = Join-Path $PSScriptRoot 'gs_qa_tests.py'
$ReportDir  = Join-Path $ProjectDir 'Saved\QAReports'

if (-not (Test-Path $Project)) { throw "cannot find the project at $Project" }
if (-not (Test-Path $Script))  { throw "cannot find the suite at $Script" }

$EngineRoot = $null
try {
    $builds = Get-ItemProperty -Path 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction Stop
    if ($builds.'5.8') { $EngineRoot = $builds.'5.8' }
} catch { }
if (-not $EngineRoot -or -not (Test-Path $EngineRoot)) { $EngineRoot = 'D:\Epic Games\UE_5.8' }
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $Editor)) { throw "cannot find UnrealEditor.exe under $EngineRoot" }

$running = @(Get-Process UnrealEditor -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    throw "an editor is already running (PID $($running[0].Id)). Close it first - the suite drives PIE and needs the project to itself."
}

if ($KeepEditorOpen) { $env:GSQA_QUIT = '0' } else { $env:GSQA_QUIT = '1' }

Write-Host ''
Write-Host 'GOBLIN SIEGE - DETERMINISTIC TEST SUITE' -ForegroundColor Cyan
Write-Host ("  maps     {0}" -f $Maps)
Write-Host ("  reports  {0}" -f $ReportDir)
Write-Host ''

$latest = Join-Path $ReportDir 'tests_latest.txt'
$stampBefore = $null
if (Test-Path $latest) { $stampBefore = (Get-Item $latest).LastWriteTimeUtc }

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
$Staged = Join-Path $Stage 'gs_qa_tests.py'

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

if (-not (Test-Path $latest)) {
    Write-Warning "the suite wrote no result. Check Saved\Logs\MyProject.log for [GSQA] lines."
    exit 1
}
$stampAfter = (Get-Item $latest).LastWriteTimeUtc
if ($stampBefore -and $stampAfter -le $stampBefore) {
    Write-Warning "tests_latest.txt was not updated - the run did not finish. Check Saved\Logs\MyProject.log for [GSQA] lines."
    exit 1
}

Write-Host ''
Get-Content $latest | ForEach-Object {
    if     ($_ -match '^\s+FAIL') { Write-Host $_ -ForegroundColor Red }
    elseif ($_ -match '^\s+PASS') { Write-Host $_ -ForegroundColor Green }
    elseif ($_ -match '^\s+SKIP') { Write-Host $_ -ForegroundColor DarkYellow }
    else                          { Write-Host $_ }
}
Write-Host ''

$summary = Select-String -Path $latest -Pattern 'passed,' | Select-Object -First 1
if ($summary -and $summary.Line -match '(\d+) failed') {
    if ([int]$Matches[1] -gt 0) { exit 1 }
}
exit 0
