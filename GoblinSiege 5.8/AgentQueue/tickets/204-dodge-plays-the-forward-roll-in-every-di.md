---
id: 204
title: Dodge plays the forward roll in every direction: add the instrument the dodge path has never had, then diagnose
agent: claude-dodgedir
status: done
claimed: 2026-08-20T17:36Z
build: required
waiting_on:
evaluated: 2026-08-20T17:39:41Z
observed: UNOBSERVED 2026-08-20T17:39:41Z - The instrument is written and cannot be exercised until a build exists - and this ticket's own claim on GSGA_DodgeRoll.cpp is what was holding the build gate shut. Same shape as 176, 179 and 189, which all closed WRITTEN-gate-closed-not-compiled. Nothing about the dodge is claimed fixed: the log has never printed a line. Closed on Michael's instruction 2026-08-20 to open the gate; the diagnosis it enables is a separate ticket.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

Dodge plays the forward roll in every direction: add the instrument the dodge path has never had, then diagnose

## Generate

The dodge path had **no instrument at all**, which is why #178's failure survived three
investigation passes. Two files:

**`Weapons/Abilities/GSGA_DodgeRoll.cpp`**
- New cvar `GS.Combat.LogDodge` (off by default, `ECVF_Default`), matching the project's existing
  `CVarLogLedge` / `CVarGSLogHitReact` pattern.
- A log line inside `PickDirectionalMontage` where both dot products are in scope, printing the
  **inputs as well as the verdict**: flattened dodge direction, the actor's forward and right
  vectors, `dotFwd`, `dotRgt`, the chosen montage, and whether the slot was null. "It chose Forward"
  on its own cannot separate a wrong *direction* from a wrong *projection*, and that ambiguity is
  exactly what the three passes could not resolve.
- The zero-input arm now announces itself. `GetDodgeDirection()` returns `GetActorForwardVector()`
  when `LastMoveInput` is zero, so "dodged without a direction held" produces a forward roll that is
  **indistinguishable from a broken picker** from outside. That is a live hypothesis and the log now
  states it outright rather than leaving it to be guessed at.

**`Combat/GSDebugCommands.cpp`** - registered in the `GSDebugToggles` table so `GS.PlayerView` can
silence it. Two toggles were once missing from that table and quietly kept talking; this one is
listed from the start.

## Evaluate

**Written, not exercised.** No behaviour is claimed. The build gate was shut by this ticket's own
claim on `GSGA_DodgeRoll.cpp`, which is the recurring shape here - #176, #179 and #189 all closed
"WRITTEN, gate closed, not compiled" for the same reason.

**Deliberately an instrument and not a fix.** Everything static about #178 already passes: the four
slots are assigned, the four montages hold distinct correct clips, they use `DefaultSlot` (the same
slot the visibly-working attack montages use), `LastMoveInput` has a writer, the projection maths is
correct by inspection, and facing is genuinely decoupled - Michael confirmed strafing works. Shipping
a speculative fix on top of that would be the fourth guess in a row. `GS.Anim.Snapshot` (#137)
settled an identical stalemate in one command after four passes of reasoning failed.

**Risk if the instrument is wrong:** it reports the picker's own locals, so it cannot lie about what
the picker saw - but it says nothing about what `GetDodgeDirection()` was given, because
`LastMoveInput` is private on `AGSPlayerCharacter`. If the log shows a sane direction and a sane
choice, the next step is upstream of this file.

## Refine

Logged the inputs rather than only the decision, after realising a verdict-only line would leave the
two candidate causes indistinguishable - which is the whole failure this ticket exists to end.

**Deliberately left undone:** the `_RM` finding from #178. Every dodge montage is built from the
root-motion clips while `GSGA_DodgeRoll.h` states the **in-place** variants are used and the
`LaunchCharacter` is kept - and the in-place variants **do not exist in the project**. That is a real
travel-distance defect, it is independent of the direction bug, and folding it in here would put two
variables in one build.
