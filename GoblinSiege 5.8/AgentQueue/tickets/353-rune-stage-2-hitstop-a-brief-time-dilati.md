---
id: 353
title: Rune stage 2: hitstop - a brief time-dilation dip on attacker and victim when a swing connects, scaled by the weight of the blow
agent: claude-combat
status: done
claimed: 2026-08-29T05:27Z
build: none
waiting_on:
evaluated: 2026-08-29T05:58:24Z
observed: 2026-08-29T05:58:10Z | Measured hitstop end to end with a per-frame CustomTimeDilation sampler armed before the swing. The swing connected (bone RightLeg) and the trace shows both attacker and victim dropping to the authored 0.05 scale on the same frame and both restoring to 1.0 on the next sample, with the final frame at 1.25s still at 1.0 - so the freeze applies to both parties and, critically, restores rather than leaking. Caveat on the instrument: Slate post-tick sampled at only ~8Hz with the viewport unfocused, so the 0.06s hold falls inside one 0.125s sample gap; the freeze value and the restore are unambiguous, the exact duration is not resolvable by this probe.
scenario: PIE on L_CombatArena, guard placed 120uu ahead of the possessed player, GS.Combat.Debug 1, sampler registered via register_slate_post_tick_callback before TryActivateAbilityByClass on the axe light attack.
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
---

## Goal

Rune stage 2: hitstop - a brief time-dilation dip on attacker and victim when a swing connects, scaled by the weight of the blow

## Generate

Michael's first complaint this session was "no reaction to my hits." #349 gave melee real hit data
and #351 gave it a sound; this is the third and largest piece of the impact layer. Before it, the
entire `Source/GoblinSiege` tree had **zero** references to any form of time dilation.

**`AGSCharacterBase::ApplyHitstop(Seconds, Scale)`** - freezes THIS character by setting
`CustomTimeDilation`, restored by `RestoreTimeDilation()` on a timer. Called on BOTH parties of a hit
from `UGSGA_SwordLight::DoSweep`, at the one site that knows both ends of the hit and which stage
landed it (the same reasoning as `NotifyDealtDamage`, which sits directly above it - the attribute
path sees a damage number and nothing else).

**Per-stage data on `FGSSwingStage`:** `HitstopSeconds` (0.06 default, 0 disables), `HitstopScale`
(0.05 - a whisper of motion so the pose does not read as a dropped frame), and
`GuardBreakHitstopScale` (2x when the hit actually broke a guard - a kick that rips a shield open
should land harder than the same kick into air). So a light, a heavy and a guard-break each carry
their own weight, and it is all tunable on the ability CDO without a build.

**Two design decisions that are the whole substance of this ticket:**

1. **Re-entrancy.** Two hits can land on one character in one frame - a three-goblin pile-on, or a
   heavy landing during a light's stop. The naive "set dilation, timer restores it" fails both ways:
   the second call caches the already-frozen value as "normal" and restores INTO the freeze,
   stranding the character in slow motion for the rest of the raid. So the pre-freeze speed is
   cached exactly once per freeze (`CachedTimeDilation`, -1 = not frozen), and a re-entrant call only
   ever EXTENDS the hold to the later of the two restores - never re-caches, never restarts. Same
   guard shape as `UGSGA_SwordLight::CachedMaxWalkSpeed`.

2. **Which clock ends the freeze.** Verified in engine source, not assumed: `CustomTimeDilation`
   scales only the actor's OWN tick (`Actor.h:796`), while the world's `FTimerManager` advances on
   the unscaled world `DeltaSeconds` (`LevelTick.cpp:1816`). So the restore is scheduled on the
   world timer manager. An ability task or a per-actor delay would be slowed by the very dilation it
   exists to end - at `Scale 0` it would never fire and the freeze would be permanent. Wall-clock is
   the only clock that can be trusted to end a freeze.

## Evaluate

**Observed:** Per-frame CustomTimeDilation trace across a connecting swing (bone RightLeg): attacker and victim both at 0.050 on the hit frame, both back at 1.000 on the next sample, final frame at 1.25s still 1.0. The freeze applies to both parties and restores cleanly - no leak. Instrument caveat: the Slate post-tick sampler ran at ~8Hz (viewport unfocused), so the 0.06s hold sits inside one 0.125s gap; freeze value and restore are proven, exact duration is not measurable this way and is a feel judgement.

**Not observed by a human, and this is a FEEL feature.** Whether 0.06s reads as weight or as lag is
exactly the kind of judgement `CaptureViewport` cannot make and a log line cannot prove. The
mechanism is measured; the feel is Michael's call and the numbers are on the CDO for him to dial.

**What the runtime measurement can and cannot show.** `CustomTimeDilation` can be read before,
during and after a hit; a value that dips and returns proves the freeze applies AND restores - the
second half being the one that matters, because a leaked freeze is the failure mode that would
surface ten minutes later as "the knight is moving in slow motion and I don't know why." The
re-entrant path (two hits inside one hold) is reasoned from the code and exercised only if the
pile-on happens during the test; it is not separately forced.

**AGENT_STATE.md owes:** DECISION - *a landed melee hit freezes both parties for a per-stage
`HitstopSeconds` via `CustomTimeDilation`, restored on the world timer manager; the impact layer now
has weight as well as sound and location.*

## Refine

**Changed in response to my own evaluation:** the restore was going to be an ability task. Reading
`LevelTick.cpp` before writing it showed that would be dilated by its own freeze, so it became a
world-manager timer. That is the one place this feature could have failed silently and permanently.

**Deliberately left undone:**

1. **Camera shake and the deflect cue** - the remaining two pieces of Stage 2. Hitstop is the
   foundation they read against; a deflect cue in particular only means something once a normal
   hit has weight to contrast with.
2. **Hitstop on ARROWS.** `AGSArrowProjectile` applies damage on its own path and does not go
   through `DoSweep`, so an arrow hit has no stop. Probably correct - a ranged hit should feel lighter
   than a blade - but it is an omission, not a decision, and is recorded as such.
3. **Only the stage's own numbers.** There is no global multiplier or cvar; a project-wide
   "hitstop intensity" dial would be one `GS.Combat.HitstopScale` away if the per-stage tuning
   proves too fiddly.
4. **The victim's hit-react montage is now also frozen** during the stop, which is intended (that is
   what sells the impact) but interacts with `HitReactCooldownSeconds` = 0.45 - the cooldown runs on
   world time while the montage does not. Worth watching for a flinch that looks clipped.
