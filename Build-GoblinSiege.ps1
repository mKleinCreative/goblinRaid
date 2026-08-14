<#
.SYNOPSIS
    Full C++ build of the Goblin Siege editor target.

.DESCRIPTION
    Wraps Epic's Build.bat with the arguments this project needs, and adds the
    checks that have actually bitten us:

      * Refuses to run while UnrealEditor.exe is open. A full build cannot write
        UnrealEditor-GoblinSiege.dll while the editor holds it, and the fallback
        is the in-editor hot reload that stranded UnrealBuildTool for four hours
        on 2026-07-30.
      * Warns if the Unreal Build Accelerator cache is unwritable, which silently
        drops the build to near-serial compilation (~6 minutes instead of under
        one).
      * Reports the real outcome from the exit code, and prints the compiler
        errors rather than making you scroll.

.PARAMETER Force
    Build anyway with the editor open. Expect it to fail on locked DLLs.

.PARAMETER Rebuild
    Full rebuild instead of an incremental build. Slow; use only when the
    incremental build behaves inexplicably.

.EXAMPLE
    .\Build-GoblinSiege.ps1

.EXAMPLE
    .\Build-GoblinSiege.ps1 -Rebuild
#>

[CmdletBinding()]
param(
    [switch]$Force,
    [switch]$Rebuild,
    [switch]$Wait,
    [switch]$IgnoreQueue
)

$ErrorActionPreference = 'Stop'

$EngineRoot  = 'D:\Epic Games\UE_5.8'
$BuildBat    = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$ProjectFile = 'D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject'
$Target      = 'MyProjectEditor'
$UbaCache    = 'C:\ProgramData\Epic\UnrealBuildAccelerator'
$QueueScript = 'D:\goblinRaid\GoblinSiege 5.8\AgentQueue\gsqueue.ps1'

function Write-Step { param($Text) Write-Host "`n=== $Text" -ForegroundColor Cyan }

# --- sanity -----------------------------------------------------------------
if (-not (Test-Path -LiteralPath $BuildBat))    { throw "Build.bat not found: $BuildBat" }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Project not found: $ProjectFile" }

# --- the agent work queue ---------------------------------------------------
# Several Claude sessions run against this repo at once. On 2026-08-04 two of them were
# editing GSPlayerCharacter.cpp inside one build window: the first link produced a DLL
# describing a source tree that no longer existed, and the only reason anyone noticed was
# comparing source mtimes against the DLL. A build is the one operation where a half-written
# file from someone else's session becomes a binary you then trust. So the gate is checked
# here, structurally, rather than left as a rule agents have to remember.
Write-Step 'Checking the agent work queue'
if ($IgnoreQueue) {
    Write-Host 'SKIPPED - -IgnoreQueue was given.' -ForegroundColor Yellow
    Write-Host 'Whatever another agent has half-written will be compiled into this build.' -ForegroundColor Yellow
}
elseif (-not (Test-Path -LiteralPath $QueueScript)) {
    Write-Host "Queue script not found at $QueueScript - cannot check." -ForegroundColor Yellow
    Write-Host 'Continuing, but nothing is protecting this build from a concurrent edit.' -ForegroundColor Yellow
}
else {
    & $QueueScript buildgate
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nBuild refused: the queue is not empty." -ForegroundColor Red
        Write-Host 'Wait for those tickets to close, or pass -IgnoreQueue if you know the'
        Write-Host 'open tickets cannot affect this build.'
        exit 4
    }
}

# --- editor must be closed --------------------------------------------------
# Ask the OS process table, NOT Get-Process. `Get-Process UnrealEditor` hands back a cached .NET
# System.Diagnostics.Process object that can OUTLIVE the process it describes: after the editor
# exits you still get an object, with HasExited=True and MainWindowTitle='', so a plain `if ($p)`
# reports "running" for an editor that has been gone for minutes. That false positive aborted a
# build as "STILL RUNNING" on 2026-08-05 when the log already ended `LogExit: Exiting`.
#
# Adding `-not $_.HasExited` is not the fix - the fix is to stop asking .NET for a cached object.
# `tasklist` queries the live table, so an exited process is simply absent.
#
# Both failure directions are expensive and neither names the editor in its error:
#   false "running" -> build skipped, and you debug stale binaries
#   false "closed"  -> UBT refuses with "Unable to build while Live Coding is active"
# (Knowledge preserved from tools/gs_editor.py, which had no callers and was removed in #115.)
Write-Step 'Checking for a running editor'
$editorRows = @(tasklist /FI 'IMAGENAME eq UnrealEditor.exe' /NH 2>$null |
                Select-String -Pattern 'UnrealEditor\.exe')
if ($editorRows.Count -gt 0) {
    if (-not $Force) {
        Write-Host "UnrealEditor is running ($($editorRows.Count) process(es))." -ForegroundColor Yellow
        Write-Host "A full build cannot replace the module DLLs while the editor holds them."
        Write-Host "Close the editor and re-run, or pass -Force to try anyway."
        exit 2
    }
    Write-Host 'Editor is running and -Force was given. Expect locked-DLL failures.' -ForegroundColor Yellow
} else {
    Write-Host 'No editor running. Good.' -ForegroundColor Green
}

# --- stranded UnrealBuildTool from a previous hot reload ---------------------
$staleUbt = Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction SilentlyContinue |
            Where-Object { $_.CommandLine -like '*UnrealBuildTool.dll*' }
if ($staleUbt) {
    Write-Host "`nA UnrealBuildTool process is already running (PID $($staleUbt.ProcessId -join ', '))." -ForegroundColor Yellow
    Write-Host 'If no build is in progress this is the stranded-UBT bug; kill it before continuing:'
    Write-Host "  Stop-Process -Id $($staleUbt.ProcessId -join ',') -Force"
}

# --- the phantom build lock, SOLVED 2026-08-02 ------------------------------
# For weeks this script has intermittently sat forever on
#   "Build.bat is already running, waiting for existing script to terminate..."
# with no compiler running and no other Build.bat process anywhere. It was never
# a stale lock. Epic's Build.bat computes its lock path as
#     set LockFile=%tmp%\<mangled-path-to-Build.bat>.lock
# and then holds it open on handle 9. Agent/MCP shells inherit an environment
# that has TEMP but NOT TMP, so %tmp% expands to nothing, the lock path collapses
# to "\D-Epic Games-...lock" at the root of the current drive, the redirect is
# denied, and Build.bat misreports that failure as "already running" - forever,
# because retrying cannot fix a missing environment variable.
# It never reproduced in an interactive console because those always define TMP.
# One line fixes it. Do not remove it.
if (-not (Test-Path Env:TMP)) {
    Set-Item -Path Env:TMP -Value (Get-Item Env:TEMP).Value
    Write-Host 'TMP was unset (agent shell); set it from TEMP so Build.bat can create its lock file.' -ForegroundColor Yellow
}

# --- stale Build.bat holding the mutex --------------------------------------
# -WaitMutex makes Build.bat block silently and forever on a lock left behind by
# a killed or wedged build. Caught live on 2026-07-31: a leftover cmd.exe from a
# dead session held it with no compiler running, and the build just sat there
# printing "waiting for existing script to terminate". Detect it and say so,
# rather than letting it look like a slow build.
Write-Step 'Checking for a stale build lock'
$staleBuild = Get-CimInstance Win32_Process -Filter "Name='cmd.exe'" -ErrorAction SilentlyContinue |
              Where-Object { $_.CommandLine -like '*Build.bat*' }
$compilers  = @(Get-Process cl, link, MSBuild -ErrorAction SilentlyContinue)
if ($staleBuild -and $compilers.Count -eq 0) {
    Write-Host 'A Build.bat process is holding the build lock, but nothing is compiling' -ForegroundColor Yellow
    Write-Host '(no cl.exe, link.exe or MSBuild). That is a stale lock, not a live build.' -ForegroundColor Yellow
    Write-Host 'This run would wait on it forever. Kill it first:'
    Write-Host "  Stop-Process -Id $($staleBuild.ProcessId -join ',') -Force"
    if (-not $Force) { exit 3 }
    Write-Host 'Continuing anyway because -Force was given.' -ForegroundColor Yellow
} elseif ($staleBuild) {
    Write-Host 'A build is already running; -WaitMutex will queue behind it.' -ForegroundColor Yellow
} else {
    Write-Host 'No stale lock.' -ForegroundColor Green
}

# --- UBA cache permissions (the reason builds take ~6 minutes) --------------
Write-Step 'Checking Unreal Build Accelerator cache'
$ubaOk = $false
if (Test-Path -LiteralPath $UbaCache) {
    try {
        $probe = Join-Path $UbaCache ('.writetest_' + [guid]::NewGuid().ToString('N'))
        [IO.File]::WriteAllText($probe, 'x')
        Remove-Item -LiteralPath $probe -Force
        $ubaOk = $true
    } catch { $ubaOk = $false }
}
if ($ubaOk) {
    Write-Host 'UBA cache is writable - parallel compilation available.' -ForegroundColor Green
} else {
    Write-Host 'UBA cache is NOT writable. The build will fall back to near-serial' -ForegroundColor Yellow
    Write-Host 'compilation (one cl.exe at a time) and take several times longer.'   -ForegroundColor Yellow
    Write-Host "Fix once, in an elevated shell:"
    Write-Host "  icacls `"$UbaCache`" /grant `"$env:USERNAME`:(OI)(CI)F`" /T"
}

# --- build ------------------------------------------------------------------
# -WaitMutex is OFF by default (2026-08-01). Twice now Build.bat has sat forever on
# "waiting for existing script to terminate" against a lock held by nothing at all -
# no cl.exe, no UnrealBuildTool, no cmd.exe. The lock is a named OS mutex, so the
# stale-build check above (which looks for processes) cannot see it, and -WaitMutex
# turns that into a silent infinite hang that looks exactly like a slow build.
# Without the flag Build.bat fails fast and says so, which is always the better
# failure. Pass -Wait to queue behind a build you know is genuinely running.
$buildArgs = @($Target, 'Win64', 'Development', "-Project=$ProjectFile")
if ($Wait) { $buildArgs += '-WaitMutex' }
if ($Rebuild) { $buildArgs += '-Rebuild' }

Write-Step "Building $Target (this normally takes 1-6 minutes)"
Write-Host "  $BuildBat $($buildArgs -join ' ')`n" -ForegroundColor DarkGray

$sw  = [Diagnostics.Stopwatch]::StartNew()
$log = Join-Path $env:TEMP ('GoblinSiegeBuild_{0:yyyyMMdd_HHmmss}.log' -f (Get-Date))

& $BuildBat @buildArgs 2>&1 | Tee-Object -FilePath $log
$code = $LASTEXITCODE
$sw.Stop()

# --- outcome ----------------------------------------------------------------
$elapsed = '{0:mm\:ss}' -f $sw.Elapsed
if ($code -eq 0) {
    Write-Step "BUILD SUCCEEDED in $elapsed"
    Get-ChildItem 'D:\goblinRaid\GoblinSiege 5.8\Binaries\Win64' -Filter '*.dll' |
        Sort-Object LastWriteTime -Descending | Select-Object -First 4 Name, LastWriteTime |
        Format-Table -AutoSize
    Write-Host 'You can reopen the editor now.' -ForegroundColor Green
} else {
    Write-Step "BUILD FAILED (exit $code) after $elapsed"
    Write-Host 'Compiler errors:' -ForegroundColor Red
    $errors = Select-String -Path $log -Pattern 'error C\d+|error LNK|error :|fatal error' |
              Select-Object -First 40 -ExpandProperty Line
    if ($errors) { $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red } }
    else         { Write-Host '  (none matched - see the full log)' -ForegroundColor Yellow }
    Write-Host "`nFull log: $log" -ForegroundColor DarkGray
}

exit $code
