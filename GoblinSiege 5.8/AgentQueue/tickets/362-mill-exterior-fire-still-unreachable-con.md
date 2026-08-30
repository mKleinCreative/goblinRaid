---
id: 362
title: Mill exterior fire still unreachable: ContainsWorldLocation never overridden
agent: claude-fire
status: done
claimed: 2026-08-30T01:36Z
build: none
waiting_on:
evaluated: 2026-08-30T02:58:15Z
observed: 2026-08-30T02:58:16Z | The field mill (the one that previously failed to catch from outside) caught fire from an actual thrown torch and collapsed, matching the exact repro case reported broken. Both mills' geometry, roof and sail drops confirmed correctly matched per-instance in the log.
scenario: Live PIE, L_Tutorial_Island, exterior torch thrown at GS_Windmill (GSMillObjective_0) after rebuild, human-observed collapse
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.h
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.h
  - Source/GoblinSiege/Destruction/GSBurnObjectiveBase.cpp
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
---

## Goal

Mill exterior fire still unreachable: ContainsWorldLocation never overridden

## Generate

#361 retired the mill's exterior-fire-immune rule by rewriting `IgniteAtLocation` to call
`IgniteInterior()` instead of doing nothing. Michael reported the mill still would not catch from an
exterior torch after that shipped. Root cause: `IgniteAtLocation`'s body was never the gate.
`AGSTorchProjectile::OnProjectileHit` only calls `Objective->IgniteAtLocation(...)` on whatever
`AGSBurnObjectiveBase::FindObjectiveAtLocation` returns, and that function only returns an objective
whose `ContainsWorldLocation(WorldLocation)` answers true. The base class's `ContainsWorldLocation`
unconditionally returns `false`, and `AGSMillObjective` never overrode it - documented in THREE
separate comments (`GSBurnObjectiveBase.h` twice, `.cpp` once) as the actual mechanism enforcing
"reachable through windows only". #361 changed the wrong function.

Fix:
- `GSMillObjective.h`/`.cpp`: added `virtual bool ContainsWorldLocation(const FVector&) const override`.
  True when the point falls within the resolved geometry actor's bounds, padded 300uu (matching the
  generosity `WindowTriggerExtent` already gives the interior route). Added `mutable TWeakObjectPtr<AActor>
  CachedMillGeometry`, filled by `ResolveMillGeometry()` in `BeginPlay` and reused by both
  `ContainsWorldLocation` (called on every torch hit in the level - re-searching would be wasteful) and
  `SinkTower` (previously its own independent call), with a lazy re-resolve in `ContainsWorldLocation`
  itself if BeginPlay ran before the geometry actor existed.
- `GSBurnObjectiveBase.h`/`.cpp`: corrected the three stale "the mill answers false / refuses exterior
  fire" comments to describe the actual contract (ContainsWorldLocation gates IgniteAtLocation) instead
  of asserting a specific subclass's now-false behaviour.
- `GSBuildingObjective.h`: corrected one comment citing the windmill as still exterior-immune.

Rebuilt (`Build-GoblinSiege.ps1 -IgnoreQueue`) - succeeded in 4m33s, no new warnings.

**Found and fixed mid-session, unrelated to this ticket's files:** two separate incidents of leaked
PowerShell background processes (from earlier timed-out MCP tool calls) consuming 23-24GB of system
RAM each, which is what actually caused the earlier "editor is hung" and "Epic Games Launcher crashed
from OOM" symptoms this session hit. Not a code issue - a tooling gotcha, written up in
`GoblinSiege 5.8/CLAUDE.md`'s Editor Python gotchas section.

## Evaluate

**Verified live, direct observation, the exact scenario that was reported broken.** After the
rebuild, Michael threw an actual torch at the mill that had failed before (not the one that already
worked, and not a debug ignite-all command) and confirmed it caught fire and collapsed. This is the
precise repro case #362 exists to fix - not a re-test of the working mill, and not the debug-cheat
path that #361's own evidence turned out to have accidentally relied on (flagged as a gap in #361's
own Evaluate section: "Not separately isolated: whether the exterior-ignite path specifically... was
what lit these two mills this run" - that gap was real, and this is what it was hiding).

**Process lesson worth stating plainly:** #361 was closed `done` on strong-looking evidence (Michael
watched both mills burn) that turned out to have tested the WRONG path - both mills that session were
lit via `IgniteInterior`/window overlap, not the exterior route #361 claimed to fix. "A human watched
it" is the strongest evidence tier this project has, but it still has to be watching the SPECIFIC
thing the ticket changed, not just an adjacent success. Isolating exactly what triggered ignition
(the debug command? which route? which mill?) before declaring victory would have caught this before
Michael did.

**Not yet isolated:** whether both mills behave identically now, or whether some other asymmetry
still exists (Michael separately asked "why does one work and the other doesn't" mid-investigation,
before the fix was confirmed - that question was answered by the fix itself: ContainsWorldLocation
was missing for BOTH mills equally, so there was no real per-instance asymmetry to find, only the
window-vs-exterior route difference in how each was tested). Worth a quick confirmation pass on the
second mill's exterior route too, opportunistically, next time either is tested.

## Refine

Closing as `done`. Fix is built, and confirmed by the user directly reproducing the originally-broken
case (exterior torch on the previously-failing mill) after the rebuild. The stale comments this
ticket also corrected matter beyond tidiness: they are what led #361 to treat `IgniteAtLocation`'s
body as the whole mechanism in the first place.
