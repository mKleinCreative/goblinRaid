---
id: 239
title: Follow slots renumber when a goblin dies, so the whole horde jostles
agent: claude-acf
status: done
claimed: 2026-08-21T18:52Z
build: done
waiting_on: BUILT. Needs watching: summon a squad, kill one in the MIDDLE, and see whether the survivors hold position instead of all sliding forward one place.
evaluated: 2026-08-23T22:43:22Z
observed: 2026-08-23T22:43:19Z | Killed BP_HordeGoblin_C_4 in slot 4 of 8; the seven survivors reported slots 0,1,2,3,5,6,7 in the second dump - every one kept the slot it had and slot 4 was left as a gap, with 0 duplicate claims. Before the fix C_5/6/7 would have renumbered to 4/5/6
scenario: live PIE, eight-goblin band, killed from the middle of the formation via GS.Horde.KillSlot
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeGoblin.h
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
---
## Generate

`UGSHordeSubsystem::GetFollowSlotFor` derived a goblin's formation slot from its **index** in the
summoner's roster array, counted fresh on every call. `RemoveFromActive` compacts that array with
`RemoveAll`, so the instant one goblin died every goblin behind it inherited the slot in front - and
the whole horde shuffled forward one place at once. A death should leave a gap, not reorder the band.

- `AGSHordeGoblin` gains `FollowSlot` (`INDEX_NONE` until assigned) with a getter and setter.
- The roster add path assigns it **once**, as the lowest slot no living sibling holds. So a casualty
  frees its slot, every survivor keeps its own, and the next summon fills the hole rather than
  extending the tail.
- `GetFollowSlotFor` now just reads it, falling back to slot 0 for a goblin the roster has never seen
  - the same fallback as before, so an unregistered goblin stacks on the summoner rather than being
  sent to a formation position that does not exist.

## Evaluate

Built (00:29, Succeeded). **Not watched.** Nothing here is verified beyond compiling, and the
symptom is inherently a thing you have to SEE - a horde stepping sideways in unison.

The measurement that would settle it: summon a squad, let them form up, kill one in the MIDDLE of the
formation, and watch whether the survivors hold position. Before this change they all slid one place
forward together; after it there should be a visible gap where the casualty stood, filled only by the
next goblin summoned.

Slot reuse is deliberate and worth stating because it is a design choice, not just a fix: the
alternative was ever-increasing slot numbers, which never reuses a gap and marches the formation
further from the summoner with every casualty.

## Refine

Nothing changed on review. One thing deliberately left alone: `RemoveFromActive` still prunes stale
weak pointers opportunistically, which is now harmless - slots live on the goblins, so compacting the
array cannot renumber anything.
