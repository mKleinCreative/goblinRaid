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
    [switch]$Wait
)

$ErrorActionPreference = 'Stop'

$EngineRoot  = 'D:\Epic Games\UE_5.8'
$BuildBat    = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$ProjectFile = 'D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject'
$Target      = 'MyProjectEditor'
$UbaCache    = 'C:\ProgramData\Epic\UnrealBuildAccelerator'

function Write-Step { param($Text) Write-Host "`n=== $Text" -ForegroundColor Cyan }

# --- sanity -----------------------------------------------------------------
if (-not (Test-Path -LiteralPath $BuildBat))    { throw "Build.bat not found: $BuildBat" }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Project not found: $ProjectFile" }

# --- editor must be closed --------------------------------------------------
Write-Step 'Checking for a running editor'
$editor = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($editor) {
    if (-not $Force) {
        Write-Host "UnrealEditor is running (PID $($editor.Id -join ', '))." -ForegroundColor Yellow
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
