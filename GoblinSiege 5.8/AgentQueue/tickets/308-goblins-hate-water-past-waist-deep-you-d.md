---
id: 308
title: Goblins hate water: past waist deep you drown
agent: claude-warren
status: done
claimed: 2026-08-25T22:40Z
build: required
waiting_on: Michael: wade the village river (should be safe), then go out of your depth (should kill in ~4s).
evaluated: 2026-08-25T23:01:08Z
observed: 2026-08-26T01:15:56Z | Walked the character into deep water and it drowned and died
scenario: PIE in L_Tutorial_Island, player character entering water past depth
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/World/GSWaterVolume.h
  - Source/GoblinSiege/World/GSWaterVolume.cpp
  - Content/Maps/L_Tutorial_Island.umap
  - docs/decisions-ledger.md
---

## Goal

Michael: *"let's have anything further than waist deep will drown you, We can also make it cannon that
Goblins notoriously hate water."* Rulings 67-69.

**This replaces the coast wall.** #307 tried four algorithms to trace a shoreline and produced four
wrong shapes. Drowning closes the water side without a barrier at all - and explains itself, which no
invisible wall does.

## Generate

**Almost all of it already existed and was unreachable**, which the research subtask established
before a line was written: `AGSWaterVolume` compiles, `bCanSwim` and `MaxSwimSpeed` are set,
`HandleStaminaExhausted` -> `Drown()` destroys carried cargo and routes through the one death path.
Written in #054 (2026-08-07), with the level work deferred and never done.

**Three things were missing, and only one was code.**

**1. Nothing drained stamina in water.** `GetSwimDrainRate()` had **no callers project-wide** - a
tuning field that looked configured and did nothing, so a goblin in deep water would have swum for
ever. `AGSPlayerCharacter::UpdateWaterDrain` now reads the rate off the volume.

**GATED ON IMMERSION, NOT ON MOVEMENT MODE.** The engine puts a character into `MOVE_Swimming` the
moment it touches a water volume at any depth, so draining on `IsSwimming()` would punish ankles in
the village river. It drains only past `DrowningImmersion01` - and since a character's physics volume
is chosen by its capsule CENTRE, "more than half submerged" is literally the waist. **The rule and the
implementation are the same fact rather than a number tuned to imitate one.**

`bDrainingFromWater` tracks whether WE set the rate, so leaving the water clears our drain and never
somebody else's - sprint drives the same setter from its own path.

**2. The sea was a floor.** `Plane2` - an 11 km plane - has blocked pawns since 2026-08-06, so the
ocean was **walkable for 5.5 km in every direction**. That was the real hole in the map: no coast wall
would ever have closed it, and the fix was one collision response. Pawns now overlap it; Visibility
still blocks so ground traces still find the surface, and Camera ignores it.

**3. No volume existed.** `GS_Sea_WaterVolume` spans X -80000..40000, Y -20000..130000, with its **top
face at exactly the waterline read from `Plane2`** rather than typed in.

## Evaluate

**Michael played it before I finished testing: *"I walked for a little bit under the water before it
killed the character."*** Both halves of that are explained and one is now fixed.

**The "walking" is the missing animation, not a bug in the movement.** There is not one swim clip on
the goblin skeleton - the research enumerated every anim asset across seven folders. He IS swimming;
`MOVE_Swimming` simply has nothing to play but the run. This is the whole cost of the route and it is
art, not code.

**The delay was tuning.** 6/s against a 100 pool is ~17 seconds - long enough to cross things. Now
**25/s, about four seconds**: long enough to panic and turn back, far too short to swim anywhere. On
the placed volume, so it moves without a build.

**Verified in play:** the water volume's top matched the plane to 0.1 uu; standing in shallows kept the
player in `MOVE_WALKING` in the `DefaultPhysicsVolume` with stamina untouched, which is wading working.

**Also found:** the landscape does not exist under the sea - there is no seabed. So with `Plane2` no
longer blocking, the water volume is **load-bearing**: without it, walking off the beach is a fall into
the void rather than a swim.

**NOT ESTABLISHED:** nobody has drowned since the retune, and nobody has tested the village river -
the case the waist rule exists to protect. If that river is deeper than half a goblin, wading it now
kills.

## Refine

**The dead tuning fields are alive rather than deleted.** The alternative was retiring
`AGSWaterVolume` and `SwimDrainRate` as an abandoned feature. Michael's ruling turned them into the
map edge instead - #054's work was correct and merely unreachable.

**Deliberately not done:** no warning before the drain starts, no splash or thrash montage to make the
death legible, no underwater post-process, and `RegenDelaySeconds` is still 0 - so a player can dip in
and out repeatedly and never drown. That last one is a real exploit and is the first thing to fix if
this survives contact.
