---
id: 295
title: ACF migration stage 5: retire whatever of the mesh path is provably dead
agent: claude-warren
status: done
claimed: 2026-08-24T22:57Z
build: required
waiting_on: #293
evaluated: 2026-08-24T23:12:17Z
observed: UNOBSERVED 2026-08-24T23:12:23Z - Nothing to watch: the ticket's conclusion is that stage 5 should remove no code, so no code changed and no behaviour moved. The analysis behind it was read from the character Blueprints and from PIE, not assumed.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
---

## Goal

Stage 5 of ruling 53: remove whatever of `UGSWeaponComponent`'s mesh path is genuinely unused. The
plan's own condition: *"Not before, and only what is provably dead - the torch, horn and quiver still
use it."*

**Blocked on #293**, which holds both files. The read-only half was done anyway, and it changes the
recommendation.

## Generate

Nothing. **The recommendation is that stage 5 removes nothing right now**, and this ticket exists to
record why rather than to leave the question open for the next session to re-ask.

## Evaluate

**Who is still on the legacy mesh path**, read from the character Blueprints rather than assumed:

```
BP_CastleGuard01/02   ACF     BP_KnightDPelegrini  ACF     BP_HordeGoblin  ACF
BP_ErikaArcher        ACF     BP_GSPlayerCharacter ACF
BP_PeasantMan         LEGACY  BP_GS_TargetDummy    LEGACY
```

And at runtime on `L_CombatArena`, every migrated character builds **no** static mesh of its own -
except the player, who still carries `GS_Quiver@spine_quiver`.

So, component by component:

| component | dead? | who keeps it alive |
|---|---|---|
| `MeleeMeshComponent` | **no** | `BP_PeasantMan` (the bucket, #275) and `BP_GS_TargetDummy` |
| `QuiverMeshComponent` | **no** | the player, and it must never become an ACF item |
| `HeldTorchMeshComponent` | **no** | the torch is a held prop, deliberately never an ACF consumable |
| `HornMeshComponent` | **no** | never migrated |
| `RangedMeshComponent` | **no live user** | but see below - "no user" is not "safe to delete" |

**`RangedMeshComponent` is the only candidate, and deleting it is still wrong.** Three reasons:

1. **It is the rollback for stages 2, 3, 4, 4b and this week's defender work.** `bUseACFEquipment` is
   a per-character flag whose whole value is that flipping it off restores the previous behaviour -
   that is what made each stage independently revertable, and it is how Erika's bow was proven not to
   be a migration regression **today**, by running her on both paths and comparing. Delete the legacy
   half and that flag becomes a switch with one position.
2. **`GetActiveWeaponMesh()` is `BlueprintPure`** and has no C++ callers, so a Blueprint may be
   calling it and a grep cannot prove otherwise. It already returns null for an ACF-held weapon, which
   its own header documents as normal - meaning any Blueprint melee trace hanging off it has been
   silently degrading since stage 2 and nobody would have seen a log line.
3. **Erika's bow is still wrong and unfixed.** #293 built the loop to fix it; until somebody has used
   that loop, the ability to put a character back on the old path is worth more than the tidiness.

**Nothing was measured about performance**, and nothing suggests this code costs anything. It is dead
weight in the reading sense, not the running sense.

## Refine

**The stage is worth NOT doing, and that is a result rather than a failure.** Ruling 59 says a defect
is judged by what the primary player experiences; unused C++ that nobody can see is below that line by
definition, and Michael's *"We NEED to get moving on the actual demo"* is the same instruction from
the other direction.

**The condition for re-opening it is concrete**, so this does not become a standing question: retire
the legacy path once `BP_PeasantMan` and `BP_GS_TargetDummy` are on ACF **and** Erika's bow has been
fixed through #293's loop. At that point `MeleeMeshComponent` and `RangedMeshComponent` are both
genuinely unreachable and the rollback has stopped earning its keep.

**Deliberately not done:** no code was edited, which is also what #293's file claim required.
