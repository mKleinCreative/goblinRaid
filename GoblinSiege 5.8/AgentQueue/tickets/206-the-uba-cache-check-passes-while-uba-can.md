---
id: 206
title: The UBA cache check passes while UBA cannot write: it probes a NEW file at the root, UBA fails on admin-owned pre-existing ones
agent: claude-ubacheck
status: done
claimed: 2026-08-20T19:41Z
build: none
waiting_on:
evaluated: 2026-08-20T19:43:58Z
observed: 2026-08-20T19:43:57Z | the rewritten check printed 'UBA cache is NOT usable: cannot open the existing memgroups file for write' against the live cache, failing on its first probe for the right reason - where the old check printed 'UBA cache is writable' in the same build run that UBA filled with Access is denied errors and validated its storage at size 0b
scenario: PowerShell in D:\goblinRaid, the UBA stage extracted from Build-GoblinSiege.ps1 and run standalone against C:\ProgramData\Epic\UnrealBuildAccelerator as the non-elevated doomsday\michael, cross-read against the 12m17s build log from 2026-08-20 12:18
files: 
  - Build-GoblinSiege.ps1
  - AGENT_STATE.md
---

## Goal

The UBA cache check passes while UBA cannot write: it probes a NEW file at the root, UBA fails on admin-owned pre-existing ones

## Generate

**The bug was in the check, not in UBA.** `Build-GoblinSiege.ps1`'s UBA stage created a **brand new
file at the cache root** and deleted it. That always succeeds — ProgramData's default ACL lets any
user create files — so the script printed *"UBA cache is writable - parallel compilation available"*
in the very same run where UBA logged ~20 `Access is denied` failures and validated its storage at
**size 0b**.

**UBA never creates fresh files at that root.** It **rewrites** `memgroups`, **replaces**
`cas/casdb`, and **deletes** stale session folders. Measured 2026-08-20: every entry under
`C:/ProgramData/Epic/UnrealBuildAccelerator` is owned by `BUILTIN\Administrators` from an elevated
install, while the build runs as a non-elevated `doomsday\michael` — who can create *beside* those
entries and cannot touch them. That is the whole gap the old probe stepped over.

**Two changes:**

1. **`Build-GoblinSiege.ps1`** — the check now probes what UBA actually does, in the order UBA does
   it: open the **existing** `memgroups` for write (the thing it retries 20s for), write-and-delete
   inside **`cas`** (needs delete rights for the `casdb` move), and check **session folder ownership**
   (each foreign-owned one is a directory that never gets reclaimed). The failure message now says
   the cache is dead, that it is a **speed** problem and not a correctness one, and gives both fix
   commands.
2. **`AGENT_STATE.md`** — the pre-seed line reading *"the UnrealBuildAccelerator permission problem is
   FIXED as of 2026-08-07"* is struck through and corrected. **It was wrong the day it was written**,
   because it was believed on the strength of the lying green line. Worse, it explicitly told readers
   that `CLAUDE.md`'s "UBA is crippled" section was stale — **`CLAUDE.md` was right all along**, so
   that file needs no edit and is left untouched.

## Evaluate

**Watched:** the new check, extracted and run against the live cache, prints
`UBA cache is NOT usable: cannot open the existing memgroups file for write.` — it fails on the first
probe, for the correct reason, on the real machine. The old check's failure is evidenced in the
2026-08-20 build log, which carries the green "writable" line and UBA's `Access is denied` errors
**in the same run**.

**Cost measured, not asserted:** that build took **12m17s** against the 4:45 the project expects,
with `Validated storage (size 0b)` — the cache contributed nothing.

**NOT fixed, deliberately: the permissions themselves.** `icacls` on admin-owned entries needs an
**elevated** shell and this session is not elevated (verified — `IsInRole(Administrator)` returned
False). Attempting it non-elevated would apply partially across `/T` and leave the ACLs inconsistent,
which is worse than leaving them alone. Handed to Michael as one command.

**What this does not prove:** that fixing the ACLs restores build speed. The 4:45 figure comes from
#069 and was not re-measured here. The next build after the elevated fix is the test.

## Refine

Wrote the check as **three ordered probes** rather than one, so the failure message can name which
one failed. A single boolean would have reproduced the original sin at higher resolution: knowing the
cache is unusable without knowing why is barely better than a green line that lies.

Struck the AGENT_STATE line through rather than deleting it, and recorded *why* it was believed. The
durable lesson is not the ACL — it is that **a probe testing something easier than what the real code
does will pass forever**, which is the same failure as `ClimbBlockedSeconds` reading 0.000 for 1285
ticks while the character was visibly stuck.

**Left undone:** the elevated `icacls` plus the `sessions` clear, and the re-measurement afterwards.
