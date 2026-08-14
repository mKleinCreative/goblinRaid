---
id: 088
title: Stage 2: the horn actually summons - BP_HordeGoblin, BT/BB, class path, arrival markers, MMB
agent: claude-horn
status: done
claimed: 2026-08-09T00:17Z
build: none
waiting_on:
evaluated: 2026-08-09T00:35:48Z
files: 
  - Content/Blueprints/BP_HordeGoblin.uasset
  - Content/AI/BB_HordeGoblin.uasset
  - Content/AI/BT_HordeGoblin.uasset
  - Content/AI/BP_GSHordeAIController.uasset
  - Content/AI/DA_Race_Goblin.uasset
  - Config/DefaultGame.ini
  - Content/Input/IA_Horn.uasset
  - Content/Input/IMC_Default.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Stage 2: the horn actually summons - BP_HordeGoblin, BT/BB, class path, arrival markers, MMB

## Generate

The entire editor half of the horde, which is everything #069 could not do with the gate shut. No
C++ - all of it was already compiled and waiting for assets that did not exist.

- **`/Game/Blueprints/BP_HordeGoblin`** - duplicated from `BP_GS_TargetDummy` and reparented to
  `AGSHordeGoblin`. That dummy was already 90% of the answer: `GOB_Scout_v3` on the ScoutV2
  skeleton, `ThirdPerson_AnimBP_Gob`, all four ability classes and the hit-react/block-react
  montages. The reparent preserved every one of them, and `AIControllerClass` correctly picked up
  `AGSHordeGoblin`'s constructor default. Set `RaceData = DA_Race_Goblin`, row `HordeGoblin` (40 HP);
  **cleared `GuardBreakAbilityClass`**, which is the design (`GSCharacterBase.h:228-231`), not an
  omission - goblins answer a raised guard with `State.Recoil` from #087 instead.
- **`/Game/AI/BB_HordeGoblin`** - `TargetActor`, `TargetLocation`, `TargetIsAttacking`,
  `FollowTarget`, `FollowSlot`, `HordeState` (enum `EGSHordeState`). The names are fixed by
  `AGSHordeAIController`'s key properties; a rename is a silent no-op at runtime, not an error.
- **`/Game/AI/BT_HordeGoblin`** - root Selector, hand-authored via Python:
  `[Block (gated on TargetIsAttacking, LowerPriority abort), MeleeAttack, MoveTo TargetLocation
  (gated on TargetActor), MoveTo FollowTarget, Wait]`, with `BTService_AcquireTarget` on the root and
  **`bSelectTarget = false`** - the controller owns `TargetActor` from the threat registry, and a
  scan here would fight it every tick. Goblin `TelegraphBlockChance` 0.35: quick, not trained.
- **`/Game/AI/BP_GSHordeAIController`** - a Blueprint child purely to hold `CompanionBehaviorTree`,
  which is `EditDefaultsOnly` on the C++ class. Without it every summon hits "this goblin has no
  brain" at `GSHordeAIController.cpp:41`.
- **`Config/DefaultGame.ini`** - the `[/Script/GoblinSiege.GSHordeSubsystem]` section that did not
  exist, with `HordeGoblinClassPath`.
- **Three `Marker.HordeArrival` markers** in `L_CombatArena`. Fixing the class path alone only moves
  the failure one step later, to `FindArrivalTransform`.
- **`IA_Horn` + a MiddleMouseButton row in `IMC_Default` + `HornAction` on `BP_GSPlayerCharacter`.**
  MMB not G: G has been `IA_Block` since the #058 remap. `HornAbilityClass` needed nothing - it is
  C++-defaulted.

## Evaluate

**THE HORN SUMMONS, AND THE SUMMONED GOBLINS FIGHT. PIE-verified on `L_CombatArena`.**

```
LogGSHorde: Horn: 4 answered. Reserve 16, active 4/10.
[GS.AI]     BP_HordeGoblin_C_0: telegraph SEEN from BP_KnightDPelegrini_C_3
[GS.Damage] BP_KnightDPelegrini_C_3 -> BP_HordeGoblin_C_2  raw 25.0  BLOCKED - armor 0.0  = 5.0
[GS.AI]     BP_HordeGoblin_C_2: block LANDED - BP_KnightDPelegrini_C_3 is open, dropping guard
[GS.Damage] BP_HordeGoblin_C_2 -> BP_KnightDPelegrini_C_3  raw 25.0  - armor 6.0  = 19.0
```

A summoned goblin read a knight's wind-up, blocked it, the knight's swing was cancelled and left him
open, and the goblin hit him for it. `UGSGA_Horn` itself had never executed a line before today.

Tally over the horde fight: 77 telegraphs seen, 32 block rolls (9 accepted at the goblins' 0.35 and
the knights' 0.55), 7 hits resolving BLOCKED, 7 recoil punishes opened, 38 damage events. Knights
down to 18/75, goblins to 10/40, goblins dying.

**Pool accounting is correct against decision 40**, which is the thing most likely to have been
subtly wrong: `Horde goblin died. Reserve 10 (unchanged), active 6/10` - debited at spawn only, death
credits nothing back. A second blast then read `Horn: 4 answered. Reserve 6, active 9/10`, correctly
clamped by the cap.

**Not verified:**
- **The horn has only ever been fired by `GS.Horde.SpawnTest`, never by a human pressing MMB.** The
  binding, the ability, the alarm-raise and the montage-less blast are all wired but the input path
  itself is unproven. This is the single most likely thing still to be broken, and it is one press
  to find out.
- The alarm rising to `Raid` on the blast's first frame was not checked.
- `L_Tutorial_Island` has **no arrival markers** - the horn will summon nothing there. Only the arena
  was set up.
- Goblins die in two knight hits (40 HP vs 25 damage). That is the archetype numbers behaving, not a
  bug, but a warband melts against knights and wants either more goblins or fewer knights.
- Guard break still 0. Superseded for AI by #087's recoil, but still never observed firing.

## Refine

**Two editor-API traps cost time and are worth the next agent's attention.** `AcceptableRadius` and
`WaitTime` are `ValueOrBBKey_Float` structs in 5.8, not floats - set `DefaultValue` on the struct and
write it back. And the concrete blackboard key-type properties are **PascalCase only** through
Python: `BaseClass`, `EnumType`. `base_class` raises "failed to find property"; the snake_case alias
does not exist for these.

Every asset write here used `save_loaded_asset(asset, False)` after #085's lesson - the default
`only_if_is_dirty=True` silently wrote nothing when the edit had not dirtied the package.

**The ini needed an editor restart.** `Config = Game` properties are read at class-default load, so
appending the section while the editor was running changed nothing and the summon still reported
`HordeGoblinClassPath is unset`. Also normalised the file back to CRLF - the append had introduced
five bare LFs into an otherwise CRLF file.

**Deliberately left undone:** arrival markers on `L_Tutorial_Island` (a 185 MB LFS save, and the
arena is where tuning happens); the horn montage (`HornMontage` is a soft pointer with no asset - its
absence costs the wind-up animation, not the feature); and the point command with its `Commanded` /
`Stranded` / `PanicStranded` states, which still have no writer anywhere.
