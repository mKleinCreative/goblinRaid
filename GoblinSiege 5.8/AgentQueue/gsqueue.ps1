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
    [string]$Note
)

$ErrorActionPreference = 'Stop'

$QueueDir  = $PSScriptRoot
$TicketDir = Join-Path $QueueDir 'tickets'
$BoardFile = Join-Path $QueueDir 'QUEUE.md'
$RepoRoot  = (Get-Item -LiteralPath $QueueDir).Parent.Parent.FullName

# Open = still holds its file claims AND still blocks the build gate.
# Closed = releases its claims and lets the build through.
$OpenStatuses   = @('queued', 'active', 'review', 'blocked')
$ClosedStatuses = @('done', 'abandoned')

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
            }
        }
    }

    $num = 0
    [void][int]::TryParse($id, [ref]$num)

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
        Files     = $fileList
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
    if (-not $hit) { throw "Ticket $Path has no '$Key' field." }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
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
            [void]$sb.AppendLine("| $($t.Id) | $($t.Status)$w | $($t.Agent) | $($t.Title) | $f | $($t.Build) |")
        }
    }
    [void]$sb.AppendLine('')

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
    Set-Content -LiteralPath $BoardFile -Value ($head + (Format-Board) + $tail) -Encoding UTF8
}

# ----------------------------------------------------------------- commands --

function Invoke-List {
    Write-Output (Format-Board)
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
    if (-not $Id)     { throw 'set needs -Id.' }
    if (-not $Status) { throw 'set needs -Status.' }
    $t = Get-TicketById $Id
    if (-not $t) { throw "No ticket #$Id." }

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

    $script:Status = 'done'
    Invoke-Set
}

function Invoke-BuildGate {
    $open = @(Get-Tickets | Where-Object IsOpen)
    if ($open.Count -gt 0) {
        Write-Output "BUILD GATE CLOSED - $($open.Count) ticket(s) still open:"
        foreach ($t in $open) { Write-Output "  #$($t.Id) [$($t.Status)] $($t.Agent): $($t.Title)" }
        Write-Output 'Do not build. Wait for these, or have them abandon.'
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
  done -Id <n>                                  close (refuses unless G/E/R are written)
  buildgate                                     exit 0 only if nothing is open
  render                                        rewrite the board in QUEUE.md

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
