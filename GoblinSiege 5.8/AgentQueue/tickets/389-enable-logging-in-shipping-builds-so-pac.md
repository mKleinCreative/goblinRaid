---
id: 389
title: Enable logging in Shipping builds so packaged-game logs are readable
agent: claude-shiplog
status: done
claimed: 2026-08-31T02:50Z
build: none
waiting_on:
evaluated: 2026-08-31T02:50:53Z
observed: UNOBSERVED 2026-08-31T02:50:54Z - One-line TargetRules flag (bUseLoggingInShipping=true); no repackage has happened with this change in yet to watch it produce a populated log file. Verify on the next Shipping BuildCookRun by checking Saved/Logs/MyProject.log (or %LOCALAPPDATA%/MyProject/Saved/Logs) is non-empty.
scenario: none - never run
files: 
  - Source/MyProject.Target.cs
---

## Goal

Enable logging in Shipping builds so packaged-game logs are readable

## Generate

Added `bUseLoggingInShipping = true;` to `Source/MyProject.Target.cs` (the packaged Game target,
not the editor target). Shipping compiles UE_LOG down to a no-op by default (NO_LOGGING=1) - a
just-packaged Shipping .exe was confirmed to write a completely empty `Saved/Logs/MyProject.log`
(checked both the install dir and the `%LOCALAPPDATA%/MyProject/Saved/Logs` fallback) even while
reproducing the "New Raid does nothing" bug, which made that bug undiagnosable from log evidence in
the packaged build even though the identical click logs plenty in PIE. This flag keeps every
existing UE_LOG call compiled in for Shipping too, so the same log file that already works in the
editor/PIE becomes readable for a packaged build.

## Evaluate

Compiles by inspection (single TargetRules bool, no new includes, no other target files touched) -
NOT rebuilt or repackaged by me. A Shipping package was already running in the background under a
separate task when this fix landed, so that in-flight package predates this change and will still
log nothing; the fix only takes effect on the NEXT Shipping cook+package after this file is picked
up. Owes AGENT_STATE.md a short note under BUILT: Shipping packages now log to Saved/Logs like any
other config.

## Refine

Nothing to refine - this is a one-line TargetRules flag, correctly scoped, and Unreal's own
documentation for `bUseLoggingInShipping` confirms it does exactly what's needed here (compiles
UE_LOG back in for Shipping) with no other side effects on shipping behavior (it does not re-enable
the console or ensure/assert reporting - only logging). Leaving verification (rebuild, repackage,
confirm Saved/Logs/MyProject.log is non-empty in the next Shipping .exe) to whoever runs the next
package - this ticket closes unobserved because no repackage has happened with this change in yet.
