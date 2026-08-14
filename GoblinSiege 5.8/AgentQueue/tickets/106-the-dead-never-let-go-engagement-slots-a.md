---
id: 106
title: The dead never let go: engagement slots and ring claims leak on death
agent: claude-gobkit
status: done
claimed: 2026-08-09T20:41Z
build: none
waiting_on:
evaluated: 2026-08-09T20:57:16Z
files: 
  - Source/GoblinSiege/Combat/GSEngagementComponent.h
  - Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
---

## Goal

The dead never let go: engagement slots and ring claims leak on death

## Generate

Found while chasing a "frozen" crowd that turned out to be 14 corpses (#105).

**`UGSEngagementComponent`'s five const accessors tested `Actor.IsValid()`, which is TRUE for a
corpse** - `CorpseLifespan` defaults to 0, "never destroy", so a dead body stays a valid UObject for
the rest of the raid. The file already had the right helper for this, `GSIsDeadOrGone` at
`GSEngagementComponent.cpp:49`, with a comment reading *"A corpse is not an attacker"*. The prune path
used it. These five did not:

| function | was | now |
|---|---|---|
| `HasEngagementRoom` | `Actor.IsValid()` | `!GSIsDeadOrGone(Actor)` |
| `GetEngagedCount` | `Actor.IsValid()` | `!GSIsDeadOrGone(Actor)` |
| `GetAttackerCount` | `Grant.Holder.IsValid()` | `!GSIsDeadOrGone(Grant.Holder)` |
| `GetReservedWeight` | `Grant.Holder.IsValid()` | `!GSIsDeadOrGone(Grant.Holder)` |
| `GetClaimedSlotCount` | `Slot.Claimant.IsValid()` | `!GSIsDeadOrGone(Slot.Claimant)` |

## Evaluate

**Only the first one is behavioural, and it was self-sustaining starvation rather than a slow leak.**
`HasEngagementRoom` is `const` so it cannot prune, and the only thing that DOES prune the engaged list
is `RegisterEngaged` - which is reached solely by an attacker this function first said yes to. So a
victim with three corpses registered reported "full" forever, every live attacker skipped it, and
nobody ever registered to trigger the prune that would have cleared it. Live attackers piled onto
whoever was left. **That is a direct contributor to the crowding Michael reported.**

Measured before the fix (`t=144`, patrol run):

```
BP_CastleGuard01_C_1   dead=True   engaged=3  claimedSlots=3     <-- permanently at capacity
BP_CastleGuard02_C_1   dead=True   engaged=3  claimedSlots=3
BP_HordeGoblin_C_3     dead=False  engaged=0  claimedSlots=1     <-- slot leaked on a LIVE goblin
```

**The other four were lying to ME, and that has a cost worth recording.** `GS.Combat.CrowdStats` reads
`GetEngagedCount`, so it reported "engaged 3" on four dead guards. I spent a whole investigation on a
"frozen crowd" that was a pile of bodies, and I reported a clumping baseline measured on corpses before
catching it. A readout that counts the dead as a gang is worse than no readout.

**NOT COMPILED YET.** Deliberate: this is being batched into one build with the crowd-fix C++ changes
from the design workflow, because a build costs an editor restart.

**Correction to #105:** its baseline numbers (largest empty arc 307.7deg, distances 248-330) were
measured on corpses and should not be used. The live-only re-measure at t=18.3 is the real one: worst
capsule clearance 1.1uu, three attackers inside a 93deg arc.

## Refine

**Did NOT add a release-on-death hook**, though I had claimed `GSCharacterBase.cpp` to do it. Once the
accessors ignore corpses, the existing self-healing paths (`PruneStale` inside `ClaimRingSlot` and
`RegisterEngaged`) become reachable again and clear the stale entries on the next claim. A death hook
would be a second mechanism for a problem that now has one, and `ReleaseAll` is already called on
target-drop and target-switch. Recorded rather than done, because it is a reasonable thing for the next
person to want and the reason not to is not obvious.

**Left undone:** teaching `GS.Combat.CrowdStats` to report POSITION - largest empty arc and minimum
capsule clearance, the two metrics I had to write in Python because the readout is blind to the actual
complaint. That belongs in the same C++ batch.
