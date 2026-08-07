#Requires -Version 5.1
<#
    gsqueue.ps1 - the Goblin Siege agent work queue.

    Read AgentQueue/QUEUE.md for the protocol. This script is the only thing that
    writes ticket state, so agents never hand-edit frontmatter and never race on a
    shared file.

    Every command is safe to run from the MCP bridge: there is no '$' on any
    command line you have to type, because all of it lives in here.

        & ".\AgentQueue\gsqueue.ps1" list
        & ".\AgentQueue\gsqueue.ps1" claim -Agent aim-arc -Title "Arc materials" -Files "Content/UI/WBP_GSPlayerHUD.uasset" -Build
        & ".\AgentQueue\gsqueue.ps1" check -Id 007
        & ".\AgentQueue\gsqueue.ps1" set -Id 007 -Status active
        & ".\AgentQueue\gsqueue.ps1" done -Id 007
        & ".\AgentQueue\gsqueue.ps1" buildgate
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('list', 'claim', 'check', 'set', 'done', 'buildgate', 'render', 'help')]
    [string]$Command = 'list',

    [string]$Id,
    [string]$Agent,
    [string]$Title,
    [string[]]$Files,
    [ValidateSet('queued', 'active', 'review', 'done', 'blocked', 'abandoned')]
    [string]$Status,
    [switch]$Build,
    [string]$Note,
    [string]$WaitingOn,
    [double]$StaleHours,
    [switch]$Reaffirm
)

$ErrorActionPreference = 'Stop'

# $PSBoundParameters inside a function is that FUNCTION's bound parameters, not the
# script's - and every Invoke-* here is parameterless, so it always reads empty.
# Capture the script's at script scope and test against this instead.
$PassedArgs = $PSBoundParameters

$QueueDir  = $PSScriptRoot
$TicketDir = Join-Path $QueueDir 'tickets'
$BoardFile = Join-Path $QueueDir 'QUEUE.md'
$RepoRoot  = (Get-Item -LiteralPath $QueueDir).Parent.Parent.FullName
# D:\goblinRaid\GoblinSiege 5.8 - the Unreal project, one level below the git root.
# Both roots are needed because tickets record BOTH conventions and always have: 102 claims are
# project-relative (Source/..., Content/...) and 24 are repo-relative (tools/..., CLAUDE.md,
# "GoblinSiege 5.8/..."). Resolving against one root alone silently fails on the other
# see Resolve-ClaimedPath.
$ProjRoot  = (Get-Item -LiteralPath $QueueDir).Parent.FullName

# Open = still holds its file claims AND still blocks the build gate.
# Closed = releases its claims and lets the build through.
$OpenStatuses   = @('queued', 'active', 'review', 'blocked')
$ClosedStatuses = @('done', 'abandoned')

# How long an open ticket may sit before agents must raise it with Michael. Deliberately
# not shorter: real work routinely runs an hour, and an advisory that cries wolf gets
# ignored, which is worse than not having one. Override per-call with -StaleHours.
$StaleHoursDefault = 2.0

if (-not (Test-Path -LiteralPath $TicketDir)) {
    New-Item -ItemType Directory -Path $TicketDir | Out-Null
}

function ConvertTo-RepoPath {
    param([string]$Raw)
    if ([string]::IsNullOrWhiteSpace($Raw)) { return '' }
    $n = $Raw.Trim().Trim('"').Trim("'").Replace('\', '/')
    $rr = $RepoRoot.Replace('\', '/').TrimEnd('/')
    if ($n.ToLower().StartsWith($rr.ToLower() + '/')) { $n = $n.Substring($rr.Length + 1) }
    $n = $n -replace '^\./', ''
    return $n.TrimStart('/')
}

function Read-Ticket {
    param([string]$Path)

    $lines     = @(Get-Content -LiteralPath $Path -Encoding UTF8)
    $id        = ''
    $title     = ''
    $agent     = ''
    $status    = 'queued'
    $claimed   = ''
    $buildNeed = 'none'
    $waiting   = ''
    $evaluated = ''
    $fileList  = @()

    $inFm    = $false
    $inFiles = $false
    foreach ($ln in $lines) {
        if ($ln -match '^---\s*$') {
            if (-not $inFm) { $inFm = $true; continue }
            break
        }
        if (-not $inFm) { continue }

        if ($inFiles -and $ln -match '^\s+-\s+(.+?)\s*$') {
            $p = ConvertTo-RepoPath $Matches[1]
            if ($p) { $fileList += $p }
            continue
        }
        if ($ln -match '^([A-Za-z_]+):\s*(.*)$') {
            $k = $Matches[1].ToLower()
            $v = $Matches[2].Trim()
            $inFiles = ($k -eq 'files')
            switch ($k) {
                'id'         { $id        = $v }
                'title'      { $title     = $v }
                'agent'      { $agent     = $v }
                'status'     { $status    = $v.ToLower() }
                'claimed'    { $claimed   = $v }
                'build'      { $buildNeed = $v.ToLower() }
                'waiting_on' { $waiting   = $v }
                'evaluated'  { $evaluated = $v }
            }
        }
    }

    $num = 0
    [void][int]::TryParse($id, [ref]$num)

    # 'claimed' is written as UTC with a Z suffix; TryParse hands back Kind=Local, so
    # ToUniversalTime on both sides is what keeps this honest across time zones.
    $ageHours = -1.0
    if ($claimed) {
        $dt = [datetime]::MinValue
        if ([datetime]::TryParse($claimed, [ref]$dt)) {
            $ageHours = ((Get-Date).ToUniversalTime() - $dt.ToUniversalTime()).TotalHours
        }
    }

    return [pscustomobject]@{
        Path      = $Path
        Id        = $id
        Num       = $num
        Title     = $title
        Agent     = $agent
        Status    = $status
        Claimed   = $claimed
        Build     = $buildNeed
        WaitingOn = $waiting
        Evaluated = $evaluated
        Files     = $fileList
        AgeHours  = $ageHours
        IsOpen    = ($OpenStatuses -contains $status)
    }
}

function Get-Tickets {
    $found = @(Get-ChildItem -LiteralPath $TicketDir -Filter '*.md' -File -ErrorAction SilentlyContinue)
    $out = @()
    foreach ($f in $found) { $out += (Read-Ticket -Path $f.FullName) }
    return @($out | Sort-Object Num)
}

function Get-TicketById {
    param([string]$Wanted)
    $key = $Wanted.TrimStart('#').PadLeft(3, '0')
    foreach ($t in (Get-Tickets)) {
        if ($t.Id -eq $key -or $t.Id -eq $Wanted) { return $t }
    }
    return $null
}

function Set-TicketField {
    param([string]$Path, [string]$Key, [string]$Value)
    $lines = @(Get-Content -LiteralPath $Path -Encoding UTF8)
    $hit = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "^$Key\s*:") {
            $lines[$i] = "$Key`: $Value"
            $hit = $true
            break
        }
        if ($i -gt 0 -and $lines[$i] -match '^---\s*$') { break }
    }
    if (-not $hit) {
        # Tickets written before a field existed simply lack it. Insert rather than throw,
        # so a new field can be added without rewriting every ticket on disk.
        $out = @()
        $done = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if (-not $done -and $i -gt 0 -and $lines[$i] -match '^---\s*$') {
                $out += "$Key`: $Value"
                $done = $true
            }
            $out += $lines[$i]
        }
        if (-not $done) { throw "Ticket $Path has no frontmatter to add '$Key' to." }
        $lines = $out
    }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
}

# When did the agent last stand behind its Generate/Evaluate/Refine? Stamped on the move to
# `review`. `done` compares each claimed file's mtime against it - see Invoke-Done.
function Get-EvaluatedAt {
    param($Ticket)
    if (-not $Ticket.Evaluated) { return $null }
    $dt = [datetime]::MinValue
    if (-not [datetime]::TryParse($Ticket.Evaluated, [ref]$dt)) { return $null }
    $at = $dt.ToUniversalTime()

    # Legacy minute-precision stamps (yyyy-MM-ddTHH:mmZ, written before seconds were stored) mean
    # "some time in that minute". Comparing an mtime against the START of the minute flags every
    # file saved in the same minute as the stamp - which is the NORMAL case, because you save and
    # then immediately run `set -Status review`. Treat such a stamp as the END of its minute: exact
    # for the guarantee it can actually make, rather than a tolerance guessed at both ends.
    if ($Ticket.Evaluated -match '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}Z$') { $at = $at.AddSeconds(59) }
    return $at
}

# Where a claimed path actually lives. Tickets record two conventions (see $ProjRoot), so try the
# repo root first and the project root second. Repo-first matters: CLAUDE.md exists at BOTH roots,
# and ticket #010 claims "CLAUDE.md" and "GoblinSiege 5.8/CLAUDE.md" as separate files - repo-first
# is the order that resolves each to the one its author meant.
# Returns $null when the path resolves nowhere, which the caller must REPORT rather than skip.
function Resolve-ClaimedPath {
    param([string]$Rel)
    if ([string]::IsNullOrWhiteSpace($Rel)) { return $null }
    $rel = $Rel -replace '/', '\'
    foreach ($root in @($RepoRoot, $ProjRoot)) {
        $full = Join-Path $root $rel
        if (Test-Path -LiteralPath $full) { return $full }
    }
    return $null
}

# Claimed files written AFTER the Evaluate was stamped. Each one is a reason to re-read it.
function Get-FilesTouchedSinceEvaluate {
    param($Ticket)
    $at = Get-EvaluatedAt -Ticket $Ticket
    if (-not $at) { return @() }
    $late = @()
    foreach ($rel in $Ticket.Files) {
        $full = Resolve-ClaimedPath -Rel $rel
        if (-not $full) {
            # Unresolvable is NOT the same as unchanged. The previous code resolved against the
            # repo root only, so every "Source/..." claim missed, Test-Path failed, and the check
            # `continue`d - it passed silently on the majority of claimed files. A guard that
            # fails open is the exact shape of bug this check was written to catch, so an
            # unresolvable path is now surfaced instead of swallowed.
            $late += [pscustomobject]@{ Path = $rel; Modified = $null; Unresolved = $true }
            continue
        }
        $m = (Get-Item -LiteralPath $full).LastWriteTimeUtc
        if ($m -gt $at) {
            $late += [pscustomobject]@{ Path = $rel; Modified = $m; Unresolved = $false }
        }
    }
    return @($late)
}

# ---------------------------------------------------------------- conflicts --

# Tickets AHEAD of you (lower id) that are still open and claim a file you claim.
function Get-Blockers {
    param([string[]]$Paths, [int]$MyNum = [int]::MaxValue)
    $hits = @()
    foreach ($t in (Get-Tickets)) {
        if (-not $t.IsOpen) { continue }
        if ($t.Num -ge $MyNum) { continue }
        $shared = @()
        foreach ($p in $Paths) {
            foreach ($tp in $t.Files) {
                if ($tp.ToLower() -eq $p.ToLower()) { $shared += $p }
            }
        }
        if ($shared.Count -gt 0) {
            $hits += [pscustomobject]@{ Ticket = $t; Shared = @($shared | Select-Object -Unique) }
        }
    }
    # EVERY caller must wrap this in @(). A single blocker unrolls on return to a bare
    # PSCustomObject whose .Count is $null, so `-gt 0` is False and the one conflict
    # that matters passes silently. (Do not "fix" that here with `return ,@($hits)` -
    # on an empty $hits that yields an array containing an empty array, Count 1, and
    # you get a phantom blocker instead. Both failures were live bugs on 2026-08-05.)
    return @($hits)
}

# -------------------------------------------------------------------- stale --

function Get-StaleLimit {
    if ($PassedArgs.ContainsKey('StaleHours')) { return $StaleHours }
    return $StaleHoursDefault
}

function Get-StaleTickets {
    $limit = Get-StaleLimit
    return @(Get-Tickets | Where-Object { $_.IsOpen -and $_.AgeHours -ge $limit })
}

# Printed by list / claim / buildgate - the commands an agent runs anyway. The agent is
# told to ASK, never to decide: from inside the repo a long-open ticket and a session
# that died look identical, and abandoning live work is far worse than waiting.
function Write-StaleAdvisory {
    $stale = Get-StaleTickets
    if ($stale.Count -eq 0) { return }
    $limit = Get-StaleLimit

    Write-Output ''
    Write-Output '======================================================================'
    Write-Output " STALE TICKET - ASK MICHAEL. Do not decide this on your own."
    Write-Output '======================================================================'
    foreach ($t in $stale) {
        $age = '{0:N1}' -f $t.AgeHours
        Write-Output "  #$($t.Id) [$($t.Status)] $($t.Agent): $($t.Title)"
        Write-Output "      open ${age}h (claimed $($t.Claimed)), past the ${limit}h mark"
        if ($t.Files.Count -gt 0 -and $t.Files[0] -notlike '*no-files*') {
            Write-Output "      still holding:"
            foreach ($f in $t.Files) { Write-Output "        $f" }
        }
        if ($t.WaitingOn) { Write-Output "      says it is waiting on: $($t.WaitingOn)" }
    }
    Write-Output ''
    Write-Output ' An open ticket holds its file claims and keeps the build gate shut. From'
    Write-Output ' here you CANNOT tell live work from a session that crashed - they look'
    Write-Output ' identical. Michael can. So stop and put the question to him:'
    Write-Output ''
    foreach ($t in $stale) {
        $age = '{0:N1}' -f $t.AgeHours
        Write-Output "   `"Ticket #$($t.Id) ($($t.Agent), $($t.Title)) has been open ${age}h."
        Write-Output "    Is it still being worked on, or should it be closed? Until it"
        Write-Output "    closes I can't build, or touch the files it holds.`""
    }
    Write-Output ''
    Write-Output ' Then do what he says, and nothing else:'
    Write-Output '   still live   -> leave it alone; carry on with unblocked work'
    Write-Output "   finished     -> set -Id <n> -Status done       (needs G/E/R written)"
    Write-Output '   dead session -> set -Id <n> -Status abandoned  (revert its edits FIRST)'
    Write-Output ''
    Write-Output ' Never abandon a ticket that is not yours without being told to.'
    Write-Output '======================================================================'
}

# ------------------------------------------------------------------- render --

function Format-Board {
    $tickets = Get-Tickets
    $open    = @($tickets | Where-Object IsOpen)
    $closed  = @($tickets | Where-Object { -not $_.IsOpen })

    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('### Open - in queue order (lowest id has right of way)')
    [void]$sb.AppendLine('')
    if ($open.Count -eq 0) {
        [void]$sb.AppendLine('_Queue is empty. The build gate is OPEN._')
    }
    else {
        [void]$sb.AppendLine('| # | status | agent | title | claimed files | build |')
        [void]$sb.AppendLine('|---|--------|-------|-------|---------------|-------|')
        foreach ($t in $open) {
            $f = '-'
            if ($t.Files.Count -gt 0) { $f = ($t.Files -join '<br>') }
            $w = ''
            if ($t.WaitingOn) {
                # waiting_on takes a ticket number OR prose ("a frame capture with the
                # editor focused"). Only prefix '#' when it is actually a number.
                if ($t.WaitingOn -match '^\s*#?\d+\s*$') { $w = " (waiting on #$($t.WaitingOn.Trim('#',' ')))" }
                else { $w = " (waiting on: $($t.WaitingOn))" }
            }
            $s = ''
            if ($t.AgeHours -ge (Get-StaleLimit)) { $s = " **STALE {0:N1}h**" -f $t.AgeHours }
            [void]$sb.AppendLine("| $($t.Id) | $($t.Status)$w$s | $($t.Agent) | $($t.Title) | $f | $($t.Build) |")
        }
    }
    [void]$sb.AppendLine('')

    $staleNow = Get-StaleTickets
    if ($staleNow.Count -gt 0) {
        $ids = ($staleNow | ForEach-Object { "#$($_.Id)" }) -join ', '
        [void]$sb.AppendLine("**STALE - $ids open longer than $(Get-StaleLimit)h.** Ask Michael whether each is")
        [void]$sb.AppendLine('still live before doing anything about it. Run `gsqueue.ps1 list` for the wording.')
        [void]$sb.AppendLine('')
    }

    $needBuild = @($tickets | Where-Object { $_.Build -eq 'required' -and $_.Status -eq 'done' })
    if ($open.Count -eq 0) {
        if ($needBuild.Count -gt 0) {
            [void]$sb.AppendLine("**BUILD GATE: OPEN - and $($needBuild.Count) finished ticket(s) asked for a build.**")
        }
        else {
            [void]$sb.AppendLine('**BUILD GATE: OPEN - nothing pending, no build requested.**')
        }
    }
    else {
        [void]$sb.AppendLine("**BUILD GATE: CLOSED - $($open.Count) ticket(s) still open. Do not build game files.**")
    }
    [void]$sb.AppendLine('')

    if ($closed.Count -gt 0) {
        [void]$sb.AppendLine('### Closed')
        [void]$sb.AppendLine('')
        [void]$sb.AppendLine('| # | status | agent | title |')
        [void]$sb.AppendLine('|---|--------|-------|-------|')
        foreach ($t in $closed) {
            [void]$sb.AppendLine("| $($t.Id) | $($t.Status) | $($t.Agent) | $($t.Title) |")
        }
        [void]$sb.AppendLine('')
    }
    return $sb.ToString()
}

function Update-Board {
    if (-not (Test-Path -LiteralPath $BoardFile)) { return }
    $text  = Get-Content -LiteralPath $BoardFile -Raw -Encoding UTF8
    $begin = '<!-- BOARD:BEGIN -->'
    $end   = '<!-- BOARD:END -->'
    $bi = $text.IndexOf($begin)
    $ei = $text.IndexOf($end)
    if ($bi -lt 0 -or $ei -lt $bi) { return }
    $head = $text.Substring(0, $bi + $begin.Length)
    $tail = $text.Substring($ei)
    # TrimEnd matters: Set-Content appends its own trailing newline, so without it every
    # render grows the file by one blank line.
    Set-Content -LiteralPath $BoardFile -Value (($head + (Format-Board) + $tail).TrimEnd()) -Encoding UTF8
}

# ----------------------------------------------------------------- commands --

function Invoke-List {
    Write-Output (Format-Board)
    Write-StaleAdvisory
}

function Invoke-Claim {
    if (-not $Agent) { throw 'claim needs -Agent (a short slug that identifies you, e.g. aim-arc).' }
    if (-not $Title) { throw 'claim needs -Title.' }

    $paths = @()
    foreach ($f in @($Files)) {
        foreach ($piece in ($f -split ',')) {
            $p = ConvertTo-RepoPath $piece
            if ($p) { $paths += $p }
        }
    }
    $paths = @($paths | Select-Object -Unique)

    # Atomic id allocation: New-Item without -Force throws if the file exists, so
    # two agents claiming in the same instant get different numbers.
    $slug = ($Title.ToLower() -replace '[^a-z0-9]+', '-').Trim('-')
    if ($slug.Length -gt 40) { $slug = $slug.Substring(0, 40).Trim('-') }
    $existing = @(Get-Tickets)
    $next = 1
    if ($existing.Count -gt 0) { $next = (($existing | Measure-Object Num -Maximum).Maximum) + 1 }

    $ticketPath = $null
    $ticketId   = $null
    for ($n = $next; $n -lt $next + 200; $n++) {
        $ticketId = ([string]$n).PadLeft(3, '0')
        $candidate = Join-Path $TicketDir "$ticketId-$slug.md"
        $taken = @(Get-ChildItem -LiteralPath $TicketDir -Filter "$ticketId-*.md" -File -ErrorAction SilentlyContinue)
        if ($taken.Count -gt 0) { continue }
        try {
            New-Item -ItemType File -Path $candidate -ErrorAction Stop | Out-Null
            $ticketPath = $candidate
            break
        }
        catch { continue }
    }
    if (-not $ticketPath) { throw 'Could not allocate a ticket id.' }

    $stamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mmZ')
    $buildNeed = 'none'
    if ($Build) { $buildNeed = 'required' }

    $fileBlock = '[]'
    if ($paths.Count -gt 0) { $fileBlock = "`n" + (($paths | ForEach-Object { "  - $_" }) -join "`n") }

    $body = @"
---
id: $ticketId
title: $Title
agent: $Agent
status: queued
claimed: $stamp
build: $buildNeed
waiting_on:
evaluated:
files: $fileBlock
---

## Goal

$Title

## Generate

<!-- REPLACE: what you produced. Files touched, what each change does, the calls
you made. Delete this comment when you write the section. -->

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
"@

    Set-Content -LiteralPath $ticketPath -Value $body -Encoding UTF8
    Update-Board

    Write-Output "CLAIMED #$ticketId  ($ticketPath)"
    if ($paths.Count -gt 0) {
        $blockers = @(Get-Blockers -Paths $paths -MyNum ([int]$ticketId))
        if ($blockers.Count -gt 0) {
            Write-Output ''
            Write-Output 'WAIT - tickets ahead of you already claim these files:'
            foreach ($b in $blockers) {
                Write-Output "  #$($b.Ticket.Id) [$($b.Ticket.Status)] $($b.Ticket.Agent): $($b.Ticket.Title)"
                foreach ($s in $b.Shared) { Write-Output "      $s" }
            }
            Write-Output ''
            Write-Output "Do NOT edit those files. Either work your unblocked files first, or run"
            Write-Output "  set -Id $ticketId -Status blocked   and report to the orchestrator."
        }
        else {
            Write-Output 'No conflicts. You have right of way on every file you claimed.'
        }
    }
    Write-StaleAdvisory
}

function Invoke-Check {
    $paths = @()
    foreach ($f in @($Files)) {
        foreach ($piece in ($f -split ',')) {
            $p = ConvertTo-RepoPath $piece
            if ($p) { $paths += $p }
        }
    }

    $myNum = [int]::MaxValue
    if ($Id) {
        $t = Get-TicketById $Id
        if (-not $t) { throw "No ticket #$Id." }
        $myNum = $t.Num
        if ($paths.Count -eq 0) { $paths = $t.Files }
    }
    if ($paths.Count -eq 0) { throw 'check needs -Id or -Files.' }

    $blockers = @(Get-Blockers -Paths @($paths | Select-Object -Unique) -MyNum $myNum)
    if ($blockers.Count -eq 0) {
        Write-Output 'CLEAR - no open ticket ahead of you claims any of these files:'
        foreach ($p in ($paths | Select-Object -Unique)) { Write-Output "  $p" }
        exit 0
    }

    Write-Output 'BLOCKED - wait for these tickets to close first:'
    foreach ($b in $blockers) {
        Write-Output "  #$($b.Ticket.Id) [$($b.Ticket.Status)] $($b.Ticket.Agent): $($b.Ticket.Title)"
        foreach ($s in $b.Shared) { Write-Output "      $s" }
    }
    exit 1
}

function Invoke-Set {
    if (-not $Id) { throw 'set needs -Id.' }
    if (-not $Status -and -not $PassedArgs.ContainsKey('WaitingOn')) {
        throw 'set needs -Status, -WaitingOn, or both.'
    }
    $t = Get-TicketById $Id
    if (-not $t) { throw "No ticket #$Id." }

    # `set -Status done` used to write straight to the ticket, skipping every check Invoke-Done
    # makes: the G/E/R placeholder scan, the "never passed through review" gate, and the late-file
    # comparison. It was not hypothetical - ticket 028 is on disk as `status: done` with an empty
    # `evaluated:` and three <!-- REPLACE placeholders still in it, closed without review by
    # exactly this route. `done` reaches Invoke-Set through $script:DoneChecked, so the checked
    # path still works and only the bypass is refused.
    # `abandoned` is deliberately still allowed here: abandoning means you reverted the work, and
    # demanding a finished Evaluate for it would push agents toward closing as `done` instead.
    if ($Status -eq 'done' -and -not $script:DoneChecked) {
        Write-Output "REFUSED - use 'done -Id $($t.Id)', not 'set -Status done'."
        Write-Output 'set writes the status straight to the ticket and skips every close check:'
        Write-Output '  the Generate/Evaluate/Refine placeholder scan, the review gate, and the'
        Write-Output '  late-file comparison. Ticket 028 closed unreviewed through this exact hole.'
        exit 1
    }

    # -WaitingOn takes a ticket number or plain prose. Setting it alone (no -Status)
    # is legal, so nobody has to hand-edit frontmatter to explain a stall.
    if ($PassedArgs.ContainsKey('WaitingOn')) {
        Set-TicketField -Path $t.Path -Key 'waiting_on' -Value $WaitingOn
        if (-not $Status) {
            Update-Board
            if ($WaitingOn) { Write-Output "#$($t.Id) waiting on: $WaitingOn" }
            else { Write-Output "#$($t.Id) no longer waiting on anything" }
            return
        }
    }

    if ($Status -eq 'active' -and $t.Files.Count -gt 0) {
        $blockers = @(Get-Blockers -Paths $t.Files -MyNum $t.Num)
        if ($blockers.Count -gt 0) {
            Write-Output "REFUSED - #$($t.Id) cannot go active; tickets ahead of it hold its files:"
            foreach ($b in $blockers) {
                Write-Output "  #$($b.Ticket.Id) [$($b.Ticket.Status)] $($b.Ticket.Agent): $($b.Ticket.Title)"
                foreach ($s in $b.Shared) { Write-Output "      $s" }
            }
            exit 1
        }
    }

    Set-TicketField -Path $t.Path -Key 'status' -Value $Status
    # Moving to `review` is the moment the agent says "this G/E/R is what I stand behind".
    # Stamp it, so `done` can tell whether the work carried on afterwards.
    if ($Status -eq 'review') {
        # Seconds included. Stamping to the minute and then comparing against a file mtime flagged
        # any file saved earlier in the SAME minute as the stamp - the normal case, since you save
        # and then immediately mark review - so a clean close demanded -Reaffirm for no reason.
        # Get-EvaluatedAt still reads the old minute-only stamps on existing tickets.
        Set-TicketField -Path $t.Path -Key 'evaluated' `
            -Value ((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))
    }
    if ($Note) { Add-Content -LiteralPath $t.Path -Value "`n> $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mmZ')) $Note" -Encoding UTF8 }
    Update-Board
    Write-Output "#$($t.Id) -> $Status"

    if ($ClosedStatuses -contains $Status) {
        $freed = @()
        foreach ($p in $t.Files) {
            foreach ($other in (Get-Tickets)) {
                if ($other.IsOpen -and $other.Num -gt $t.Num -and ($other.Files -contains $p)) {
                    $freed += "  #$($other.Id) $($other.Agent) is now clear on $p"
                }
            }
        }
        if ($freed.Count -gt 0) {
            Write-Output 'Released:'
            $freed | Select-Object -Unique | ForEach-Object { Write-Output $_ }
        }
    }
}

function Invoke-Done {
    if (-not $Id) { throw 'done needs -Id.' }
    $t = Get-TicketById $Id
    if (-not $t) { throw "No ticket #$Id." }

    $raw = Get-Content -LiteralPath $t.Path -Raw -Encoding UTF8
    $missing = @()
    foreach ($section in @('Generate', 'Evaluate', 'Refine')) {
        if ($raw -notmatch "(?m)^##\s+$section\s*$") {
            $missing += "$section (heading deleted)"
            continue
        }
        $seg = ($raw -split "(?m)^##\s+$section\s*$")[1]
        if (-not $seg) { $missing += "$section (empty)"; continue }
        $seg = ($seg -split "(?m)^##\s")[0]
        # Placeholders are HTML comments, not italics: real writeups here are full of
        # underscores (BTTask_MeleeAttack, M_GS_AimArc) and an italics heuristic would
        # eat genuine prose.
        if ($seg -match '(?s)<!--\s*REPLACE') { $missing += "$section (still the placeholder)"; continue }
        if (($seg -replace '(?s)<!--.*?-->', '').Trim().Length -lt 40) { $missing += "$section (empty)" }
    }
    if ($missing.Count -gt 0) {
        Write-Output "REFUSED - #$($t.Id) is not presentable yet. Fill in: $($missing -join ', ')."
        Write-Output 'Generate / Evaluate / Refine are how the orchestrator reviews you. Write them, then re-run done.'
        exit 1
    }

    # --- has the work moved on since the Evaluate was written? ----------------------------
    # Ticket #009 closed `done` describing a world eight commits out of date: it still said the
    # two lose paths were unexercised and BP_GS_RunicSite did not exist, hours after its own
    # author had committed "both lose paths verified at last". Nothing caught it, because the
    # G/E/R gate only asks whether the sections are WRITTEN, never whether they are still TRUE.
    if (-not $t.Evaluated) {
        Write-Output "REFUSED - #$($t.Id) never passed through review, so there is no point at which"
        Write-Output 'anyone stood behind its Generate/Evaluate/Refine. Run:'
        Write-Output "  set -Id $($t.Id) -Status review"
        Write-Output 'and hand it to the orchestrator, which is what QUEUE.md rule 3 asks for.'
        exit 1
    }

    $checked    = @(Get-FilesTouchedSinceEvaluate -Ticket $t)
    $unresolved = @($checked | Where-Object { $_.Unresolved })
    $late       = @($checked | Where-Object { -not $_.Unresolved })

    # Reported always, and never silently. A path that resolves nowhere was NOT checked, and
    # before Resolve-ClaimedPath that was the majority of every ticket's claims. It does not
    # refuse on its own - a deleted file or a "no-files-claimed-yet" placeholder is not a reason
    # to block a close - but "I could not look" must never again read the same as "unchanged".
    if ($unresolved.Count -gt 0) {
        Write-Output "NOTE - #$($t.Id) has $($unresolved.Count) claimed path(s) that resolve to no file"
        Write-Output "under either $RepoRoot or $ProjRoot, so they could not be checked:"
        foreach ($f in $unresolved) { Write-Output "  $($f.Path)" }
        Write-Output ''
    }

    if ($late.Count -gt 0 -and -not $Reaffirm) {
        Write-Output "REFUSED - #$($t.Id) kept working after you wrote its Evaluate ($($t.Evaluated))."
        Write-Output 'These claimed files were written AFTER that point:'
        foreach ($f in $late) {
            Write-Output ("  {0}   (modified {1}Z)" -f $f.Path, $f.Modified.ToString('yyyy-MM-ddTHH:mm:ss'))
        }
        Write-Output ''
        Write-Output 'An Evaluate that is honest about a tree that no longer exists is worse than'
        Write-Output 'no Evaluate: the orchestrator folds its "not yet done" list into AGENT_STATE.md'
        Write-Output 'and the next agent rediscovers work that is already finished.'
        Write-Output ''
        Write-Output 'RE-READ Evaluate against what is now true. Then either:'
        Write-Output "  set -Id $($t.Id) -Status review     (you changed it - re-stamps, then run done)"
        Write-Output "  done -Id $($t.Id) -Reaffirm         (you read it and it still stands)"
        exit 1
    }

    # Tells Invoke-Set that this `done` arrived through the checks above rather than straight off
    # the command line. See the guard at the top of Invoke-Set.
    $script:DoneChecked = $true
    $script:Status = 'done'
    Invoke-Set
}

function Invoke-BuildGate {
    $open = @(Get-Tickets | Where-Object IsOpen)
    if ($open.Count -gt 0) {
        Write-Output "BUILD GATE CLOSED - $($open.Count) ticket(s) still open:"
        foreach ($t in $open) {
            $age = ''
            if ($t.AgeHours -ge 0) { $age = ' - open {0:N1}h' -f $t.AgeHours }
            Write-Output "  #$($t.Id) [$($t.Status)] $($t.Agent): $($t.Title)$age"
        }
        Write-Output 'Do not build. Wait for these to close.'
        Write-StaleAdvisory
        exit 1
    }
    $want = @(Get-Tickets | Where-Object { $_.Build -eq 'required' -and $_.Status -eq 'done' })
    Write-Output 'BUILD GATE OPEN - no open tickets.'
    if ($want.Count -gt 0) {
        Write-Output "$($want.Count) finished ticket(s) requested a build:"
        foreach ($t in $want) { Write-Output "  #$($t.Id) $($t.Agent): $($t.Title)" }
    }
    else {
        Write-Output 'No finished ticket asked for a build - you may not need one.'
    }
    exit 0
}

function Invoke-Help {
    Write-Output @'
gsqueue.ps1 - Goblin Siege agent work queue

  list                                          show the board
  claim -Agent <slug> -Title <t> [-Files a,b] [-Build]
                                                take the next queue position
  check -Id <n> | -Files a,b                    who is ahead of me on these files?
  set -Id <n> -Status <s> [-Note "..."]         queued|active|review|done|blocked|abandoned
  set -Id <n> -WaitingOn "<#n or prose>"        say what is stalling you (shows on the board)
  done -Id <n> [-Reaffirm]                      close; refuses unless G/E/R are written, the
                                                ticket passed through review, and no claimed
                                                file changed after the Evaluate was stamped.
                                                -Reaffirm = "I re-read it and it still stands"
  buildgate                                     exit 0 only if nothing is open
  render                                        rewrite the board in QUEUE.md

  -StaleHours <n>   on list/claim/buildgate, flag open tickets older than n hours
                    (default 2). A flagged ticket is a question for Michael, never
                    something you close yourself.

Queue position is the ticket id. Lower id wins every file conflict.
Read QUEUE.md for the full protocol.
'@
}

switch ($Command) {
    'list'      { Invoke-List }
    'claim'     { Invoke-Claim }
    'check'     { Invoke-Check }
    'set'       { Invoke-Set }
    'done'      { Invoke-Done }
    'buildgate' { Invoke-BuildGate }
    'render'    { Update-Board; Write-Output 'Board rewritten.' }
    'help'      { Invoke-Help }
}
