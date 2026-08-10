---
id: 120
title: GS.Combat.Debug must not default to on, and record the three-in-a-row unwatched-fix failure in AGENT_STATE
agent: claude-wrapup
status: done
claimed: 2026-08-10T22:30Z
build: required
waiting_on:
evaluated: 2026-08-10T22:40:38Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - GoblinSiege 5.8/AGENT_STATE.md
---

## Goal

GS.Combat.Debug must not default to on, and record the three-in-a-row unwatched-fix failure in AGENT_STATE

## Generate

**1. `GS.Combat.Debug` defaulted to 1** (`GSGA_SwordLight.cpp:25`). It was the only debug cvar in
the project that was ON unless someone remembered to turn it off, so every session, for every
combatant, drew cyan melee trace spheres, red hit markers and on-screen combo text over the game.

The evidence is Michael's own `CombatBugs.mp4` of 2026-08-09: the debug geometry is in frame at
29:13, 29:04 and 29:02 of a recording he made to show an *animation* bug. A debug view you have to
remember to disable is one you ship by accident.

Changed to `0`, with the reasoning recorded at the declaration. `GS.Combat.Debug 1` turns it on;
`GS.PlayerView` still sweeps every channel at once.

Deliberately NOT touched: `bDrawDebugSweep` (`GSGA_SwordLight.h:163`) also defaults to true, but it
is ANDed with this master switch at `:311` and `:499`, so nothing draws now regardless. Changing a
reflected `EditDefaultsOnly` default would also silently do nothing for the three `GA_GS_Sword*`
Blueprints that may carry their own overridden value — a separate job needing an in-editor check.

**2. `AGENT_STATE.md` FAILED entry** for the pattern behind #113, #116, #117, #118 and #119: three
consecutive fixes argued from static reads and shipped without anyone watching them run, all three
wrong. Written as a pattern rather than three bug reports, because the individual bugs are closed
and the habit is what generalises. It also records that Michael diagnosed the root-snap himself and
was told no, and names the two probes that return success for inputs that do not exist.

## Evaluate

**Verified:**

- **BUILD SUCCEEDED** — figure in Refine. The only C++ change is a literal `1` -> `0`, so the risk
  was never compilation; the build is confirmation that nothing else in the working tree broke.
- The change is a default, not a removal: `GS.Combat.Debug 1` restores the previous behaviour at
  runtime with no rebuild, which is the whole point of it being a cvar.
- Both AND-sites (`GSGA_SwordLight.cpp:311`, `:499`) read `bDrawDebugSweep && GSCombatDebugEnabled()`,
  so flipping the master switch is sufficient to silence the drawing.

**NOT verified:**

- **No PIE.** Nobody has watched a fight and confirmed the spheres are gone. Given this session's
  record that is worth stating plainly rather than assuming — though the failure mode here is
  visible in the first second of any fight, and reversible from the console.
- The other two debug-command files (`GSRaidDebugCommands`, `GSBurnDebugCommands`) and their
  `#if !UE_BUILD_SHIPPING` exposure are untouched and still open from the bloat audit.

## Refine

**Nothing changed on re-reading.** The one alternative considered was flipping `bDrawDebugSweep` to
false as well, for defence in depth. Rejected: it is `EditDefaultsOnly` and reflected, so the C++
default only reaches Blueprints that never overrode it, which makes the change look complete while
possibly doing nothing — the exact class of silent half-fix this ticket's AGENT_STATE entry is about.

**Deliberately left undone**, all pre-existing and unclaimed: the eight remaining code-review
findings (recoil-timer local handle, per-hit `FReferenceSkeleton` copy, `RemoveFromActive` stale
early-return, stamina suppression not stopping drain, four in `level-gen/generate.py`), the #115
deferrals (`UAIPerceptionComponent` removal pending a `BT_Militia` check, the damage arc
de-duplication, ~95 MB of `*_Backup_*` directories, `compose_house_synthesised`), the nine orphaned
`A_MX_*_Gob` clips, and `VibeUE.uplugin`'s uncommitted edit inside the plugin's own repo.
