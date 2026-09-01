---
id: 370
title: Fire visual: scale pooled field fire volumes to overlap along the burn front
agent: claude-fire
status: done
claimed: 2026-08-30T05:17Z
build: none
waiting_on:
evaluated: 2026-08-30T09:07:56Z
observed: 2026-08-30T09:07:50Z | Michael watched the consolidated visual live twice: sprite version - little puffs, hard to tell where fire is; fluid version - sperm shaped objects swimming on a 2d plane, even more dangerous, cannot tell where fire is. Reverted bConsolidateFireVisual to false; every pooled volume draws its own flame again (pre-session state).
scenario: Live PIE test of a burning field, player watching from ground level
files: 
  - Source/GoblinSiege/Destruction/GSFireVolume.cpp
---

## Goal

Fire visual: scale pooled field fire volumes to overlap along the burn front

## Generate

Michael (2026-08-30): "pick #370/#371 back up" - he claimed these himself earlier in the session
(HANDOFF-2026-08-30.md), then got pulled onto the horn-blast bug (#375) before writing anything.
Continuing under this ticket rather than re-claiming, since it's the same body of work.

Per the handoff, "scale pooled volumes to overlap" (this ticket's literal title) and "raise
MaxFireVolumes" (#371's title) were BOTH already tried earlier this session and are already
committed (verified live: `bAutoScaleFXToRadius=true` in `ConfigurePooled`,
`MaxFireVolumes=16`, per-instance `SetRandomSeedOffset` calls all already present in
`GSFireVolume.cpp`/`GSFieldFireObjective.h` before this ticket touched anything). The handoff's own
verdict on that work: "still rows, not enough... even under a genuinely simulated Niagara Fluids
swap - because the fundamental issue was never the rendering technology." So there was nothing left
on the *literal* ticket title to do - the actual next step, which the handoff explicitly scoped but
marked NOT BUILT, is architectural: decouple the visual from the damage.

**Implemented that architecture.** Files: `Source/GoblinSiege/Destruction/GSFireVolume.h/.cpp`,
`Source/GoblinSiege/Destruction/GSFieldFireObjective.h/.cpp` (the latter's .cpp was not originally
claimed by either #370 or #371 - flagging since it's touched now; same body of work, same session).

1. **`AGSFireVolume`**: `ConfigurePooled` takes a new `bInEnableFireFX` param (default true, so
   every OTHER caller - i.e. a torch's own single fire - is unaffected). `ApplyFireFX` now skips
   only the flame component when it's false; smoke, embers and light are untouched.
2. **`AGSFieldFireObjective`**: new `bConsolidateFireVisual` (default true). When on, pooled volumes
   are told `bInEnableFireFX=false` (they still do damage, still carry light/embers/smoke exactly as
   before) and a single new component, `ConsolidatedFireFX`, is sized/repositioned every
   `CosmeticTick` to span `GetBurningLocalBounds()` - a new helper returning the burning cells'
   bounding box in the FIELD's OWN LOCAL SPACE (not world), so a yawed field gets a box aligned to
   its actual burn front. Position/scale chase their targets via `FMath::Lerp` at
   `ConsolidatedFireInterpSpeed` rather than snapping, addressing the "resize smoothness" risk the
   handoff flagged. `ConsolidatedFireFX` is a plain runtime `UNiagaraComponent` (same pattern as
   `SmokeWisps`, not a replicated actor) built/driven on every machine via `CosmeticTick`, since
   runtime components don't replicate - same reasoning `UpdateSmokeWisps`'s own comment gives.
3. **Reverted the Niagara Fluids test swap** in `GSFireVolume.cpp`'s constructor back to
   `NS_Fire_Big`, per the handoff's explicit "revert before shipping anything unless building around
   fluids specifically" - the fluids test also read as "still rows" per the handoff, so there's no
   reason to keep it now that the real fix is architectural, not a render-tech swap.

**Deliberately NOT addressed** (both flagged as open risks in the handoff, neither attempted):
disconnected fronts (two separate ignition points not yet merged would get ONE box spanning the
empty ground between them - no per-cluster splitting here) and burnout fade specifically for the
consolidated visual beyond a plain `Deactivate()` when nothing is burning (no independent fade
curve - it just cuts).

## Evaluate

**NOT verified - written, not built or seen.** Build gate has been closed this entire session by
these same tickets (#370/#371/#375) - nobody has compiled since before this work started. Everything
above is reasoning against the code and the handoff's own numbers, not an observation:
`GetBurningLocalBounds` reuses `GetCellWorldLocation`/`GetActorTransform().InverseTransformPosition`,
both already exercised by `IsWorldLocationInField` and `GetBurningCentroid` respectively, so the
transform math itself is not new; the new arithmetic (padding by `FireVolumeRadius`, converting a
half-extent to a `RelativeScale3D` against `ConsolidatedFireAuthoredHalfExtent`) has not run once.

Two live judgment calls this needs a human for, same as every FX change in this project: does the
one wide flame actually read as "one spreading mass" rather than "one big flame in the middle of a
front that's still visibly wider than it," and does the lerp-chase actually look smooth rather than
either laggy (interp too slow) or still hitchy (too fast, or the box genuinely does jump when a
front narrows suddenly).

## Refine

Handing back at `review`, not `done` - same reason as #375: I cannot self-satisfy QUEUE.md's
"observed" gate without a build. All three of this session's open tickets (#370, #371, #375) are
now written and blocking each other's build gate with nothing further any of them can do without
one - flagging to Michael directly rather than picking at it further.

**UPDATE 2026-08-30, two live looks, both bad - reverted.** First look: "the fire looks like little
puffs and it's hard to determine where it actually is" - the sprite system (`NS_Fire_Big`) scaled
non-uniformly to span a whole burning front just spreads its fixed particle count over more area,
so it reads thinner rather than bigger. Tried swapping the consolidated visual to Michael's Niagara
Fluids prototype (`NG_GS_SurfaceFireLiquid`) on the theory that a simulated fluid wouldn't have that
same "same particles, more area" problem. Second look, worse: "a bunch of sperm shaped objects
swimming on a 2d plane. even more dangerous, I can't tell where the fire actually is" -
non-uniform `RelativeScale3D` on a fluid sim's domain evidently distorts it in ways a sprite emitter
doesn't, and the result actively obscured the hazard rather than just looking mediocre.

Two failures on the same mechanism (stretch ONE Niagara component to span a bounding box), the
second one safety-relevant (a player who can't see where a damage hazard is), is where this
stopped - did not try a third asset on the same approach. **Reverted:
`bConsolidateFireVisual` defaulted back to `false`** in `GSFieldFireObjective.h` - every pooled
`AGSFireVolume` draws its own flame again, the known pre-2026-08-30 state. This is a straight
revert of the visual-suppression switch, nothing else touched (damage, light, embers on the pooled
volumes were never part of the failed experiment). The consolidated-visual code itself
(`UpdateConsolidatedFireVisual`, `GetBurningLocalBounds`, the whole component/lerp machinery) is
left in place but disabled, in case a future attempt wants a fundamentally different mechanism -
several modestly-scaled instances spread across the box, say, instead of one heavily-stretched one
- rather than starting from nothing.

Rebuilt clean, relaunched. **Original "rows of separate fires" complaint from the handoff is back
and still unsolved** - this ticket did not fix it, it only ruled out two approaches to fixing it.
Handing back at `review` - needs Michael's eyes on the reverted state before this closes, and the
underlying visual-coherence problem is still open for whoever picks it up next.
