---
id: 358
title: Fire ground work: replace N_MeteorSpawn with a real fire system
agent: claude-fire2
status: done
claimed: 2026-08-29T22:15Z
build: none
waiting_on:
evaluated: 2026-08-29T22:31:55Z
observed: 2026-08-30T00:01:35Z | Michael watched NS_Fire_Big and the retuned point light (2400) live in PIE and called it Perfect
scenario: Live PIE, L_CombatArena, viewed alongside the #359 house-fire testing
files: 
  - Source/GoblinSiege/Destruction/GSFireVolume.cpp
  - Source/GoblinSiege/Destruction/GSFireVolume.h
---

## Goal

Fire ground work: replace N_MeteorSpawn with a real fire system

## Generate

`AGSFireVolume`'s `FireSystem` default was `N_MeteorSpawn` (RPG-MagicVFXFire) - a magic/meteor
effect standing in for fire, not authored as one. Replaced it with `NS_Fire_Big` from Michael's new
Free_Fire (Vefects) pack.

- **How the pick was made, not guessed:** spawned `Free_Fire`'s own `BP_Fire` demo actor in the
  level and inspected its live components via `unreal.NiagaraComponent`/`AudioComponent` queries
  rather than reasoning from asset names. Its one `NiagaraComponent` is `NS_Fire_Big` at
  `relative_scale3d (1,1,1)`, paired with `SFX_FireBig_L` on its `AudioComponent` - the pack
  author's own "this is what a fire looks like" reference config, not a guess. Destroyed the
  inspection actor afterward.
- Checked `fixed_bounds` on all three size tiers (`NS_Fire_Small/Medium/Big`) looking for an
  authored-radius number to justify re-enabling `bAutoScaleFXToRadius`: all three report an
  **identical, unedited +-100 box** - not informative, so there is no reliable size number to
  rescale against. `bAutoScaleFXToRadius` stays `false`, matching the reasoning that was already
  there for `N_MeteorSpawn` (native scale is the only size either system was ever confirmed to
  look right at). Updated the header comment on that flag since it named `N_MeteorSpawn`
  explicitly and would otherwise mislead the next reader.
- `SmokeSystem`/`EmberSystem` (VolcanoEnvironmentVFX) untouched - not what was flagged, and I found
  no concrete reason to touch them.
- Files: `GSFireVolume.cpp` (constructor default), `GSFireVolume.h` (comment only, no value
  changed).

**Finding, deliberately not acted on:** one `FireSystem` field renders at the same native size for
every `AGSFireVolume`, whether it's a small torch-impact fire (~140uu `DamageRadius`) or a pooled
field volume up to ~320uu (`MaxFireVolumes` comment: "3 big volumes read as three tidy bonfires,
not a field ablaze"). This was already true of `N_MeteorSpawn` and is unchanged by this swap. A
real size-tier system (switching `NS_Fire_Small/Medium/Big` by `DamageRadius`) is a bigger
structural change than a default swap and wants its own ticket - noted here so it isn't lost, not
attempted.

## Evaluate

**Verified:** the asset choice itself, via the live `BP_Fire` component inspection above - that is
real evidence the pack author intends `NS_Fire_Big` at native scale as the flagship look, not a
name-based guess.

**NOT verified - no build, no PIE, by design ("ground work," per the ask):**

- The code has not compiled since this edit. `TSoftObjectPtr` means a bad path fails soft (fire
  still damages, just isn't drawn) rather than a load error, but the path string itself has not
  been round-tripped through a compile.
- Nobody has watched `NS_Fire_Big` actually render on an `AGSFireVolume` in PIE - not the torch-
  impact case, not the pooled field case. Whether it reads right at native scale against a 140uu
  `DamageRadius` sphere (let alone a 320uu pooled one, per the finding above) is completely
  unconfirmed.
- `SetFireIntensity`'s scale/light ramp (0..1 toward "peak") was written and tuned against
  `N_MeteorSpawn`'s specific shape - untested against `NS_Fire_Big`'s.

## Refine

Leaving in `review`, not `done`, matching #357's pattern. For whoever builds and looks next:

1. Build, start PIE, spawn or trigger an `AGSFireVolume` (torch impact is the simplest path) and
   confirm `NS_Fire_Big` actually reads as fire at `DamageRadius` 140 - if it's too big/small at
   native scale, the honest fix is re-enabling `bAutoScaleFXToRadius` with a real measured
   `FireSystemAuthoredRadius` (which needs someone to determine that number by eye, the same
   place-a-marker-and-look pattern used elsewhere this session - Niagara bounds could not supply
   it here).
2. Also look at a pooled field-fire volume (see `AGSFieldFireObjective`/`ConfigurePooled`) at its
   larger radius - this is where the "one native size for every scale" finding above would show up
   most, if it's a problem at all.
3. If Michael wants deliberate size tiers instead of one native-scale system, that's a new ticket
   (a second `TSoftObjectPtr<UNiagaraSystem>` field or an enum-switched default), not a re-open of
   this one.

> 2026-08-29T22:31Z Rebalance pass: Michael watched NS_Fire_Big live in PIE and called the point light overpowering the flame (LightIntensity 4800, tuned against the old N_MeteorSpawn). Cut to 2400, rebuilt, awaiting his re-check.

> 2026-08-29T23:59Z Michael re-checked the retuned light live in PIE alongside the #359 house-fire work and called it "Perfect." Closing.
