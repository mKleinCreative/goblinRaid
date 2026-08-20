---
id: 181
title: Smashable lootable barrel: BP_LootBarrel on the SM_Barrel_01 to SM_BarrelBroken swap pair
agent: claude-channelring
status: done
claimed: 2026-08-18T01:32Z
build: none
waiting_on:
evaluated: 2026-08-18T01:39:18Z
observed: 2026-08-18T01:48:46Z | Michael watched one sword swing break the barrel and swap it to SM_BarrelBroken - his words, 'the barrel looks goooodddd' - and the log recorded the break, the interactable unlock, and a completed loot: 'Interaction COMPLETED: verb Interact.Loot on BP_LootBarrel_C_0' at 01:34:26. He also noted the broken barrel still blocks movement, which is the swap path leaving collision alone by design. A LATER run refused to loot and is not yet explained - see Refine.
scenario: PIE in L_CombatArena, BP_LootBarrel_TEST at (-1850,130,-70) 198uu ahead of the PlayerStart beside BP_LootCrate_TEST; one player sword swing then hold F
files: 
  - Content/Blueprints/Interactables/BP_LootBarrel.uasset
---

## Goal

Smashable lootable barrel: BP_LootBarrel on the SM_Barrel_01 to SM_BarrelBroken swap pair

## Generate

The crate treatment applied to the barrel, as a second use of the same three properties - no new C++,
no build.

**`BP_LootBarrel`**: `SM_Barrel_01` + `UGSBreakableComponent` (`BrokenMesh = SM_BarrelBroken`,
`SmashHitPoints = 1`, `bUnlockInteractableOnBreak = true`, `bOpensBuilding = false`,
`IntactMeshComponentName = Mesh`) + `UGSInteractableComponent` (`Interact.Loot`, **shipping
`bIsAvailable = false`**, 1.0s channel, consumes, "Loot the barrel"). Placed as `BP_LootBarrel_TEST` at
(-1850, 130, -70), 198uu ahead of the PlayerStart and beside the crate so both are one walk.

**Which barrel is the pair was measured, not assumed.** The pack ships four barrels and only one broken
variant, so the pairing is a real question:

```
SM_BarrelBroken   size=(83.1, 83.1,107.5)  base_z=-4.1
SM_Barrel_01      size=(83.1, 83.1,108.1)  base_z=-4.5   mismatch  1.0 uu
SM_Barrel_02      size=(83.1,104.6,108.1)  base_z=-4.5   mismatch 22.5 uu   <- two barrels, offset centre
SM_Barrel_03      size=(83.1, 83.1,125.7)  base_z=-4.5   mismatch 18.6 uu   <- taller
SM_Barrel_04      size=(83.1, 83.1,108.1)  base_z=-4.5   mismatch  1.0 uu
```

`_01` and `_04` are interchangeable for this purpose; `_02` and `_03` are not, and swapping either to
the broken mesh would visibly shrink or halve the prop. Worth having in writing before someone dresses a
cellar with `_03` and wonders why smashing it looks wrong.

## Evaluate

**OBSERVED, and it works.** One swing broke the barrel, swapped it to `SM_BarrelBroken`, unlocked the
interactable and looted:

```
01:34:19  'BP_LootBarrel_C_0' broken (hidden + FX).
01:34:19  'BP_LootBarrel_C_0' broken open - its interactable is now available.
01:34:26  Interaction COMPLETED: verb 'Interact.Loot' on 'BP_LootBarrel_C_0'.
```

No warnings, and no `[CLIENT ONLY]` suffix, so that was a real server-side payout. Every property was
also read back off the asset rather than trusted from setter returns.

**The broken barrel still blocks, and that is the swap path behaving as written** rather than a defect.
`RetireIntactMesh` deliberately drops collision only on the HIDE path; on the swap path it leaves it
alone, and `SetStaticMesh` re-derives the body from the new mesh anyway. `SM_BarrelBroken` carries its
own convex hull at 107.5uu - essentially full height, because it is a barrel with a broken side rather
than a stump. So a smashed barrel remains a solid barrel-shaped obstacle. **Whether that is DESIRED is
Michael's call and has not been made**; it is recorded here as a known consequence, not settled.

**An unexplained refusal, and it is the honest loose end.** A later run broke both props and then would
not loot. Nothing was logged, and that silence is a flaw in #172 that this ticket surfaced: aborts were
logged at `Verbose` to stop routine key-releases drowning the completions, which means the FAILURE case -
the only one anyone debugs - prints nothing at default verbosity. The blind spot was moved rather than
removed. `Log LogGSInteract Verbose` and `GS.Interact.Debug 1` are now enabled in the editor so the next
occurrence names its reason.

**Two candidates, neither confirmed, both testable:** the broken barrel's full-height collision holding
the player outside `InteractRange` - which would make "still has collision" and "cannot loot" one fact
rather than two - or `ResolveChannelTarget` scoring the crate higher, since barrel and crate sit 120uu
apart and both become available on the same swing.

## Refine

**Changed after self-review:** the first pass simply used `SM_Barrel_01` because the earlier crate work
had measured it. Re-measured all four against the broken variant instead, which is what surfaced that
`_02` is a two-barrel arrangement - a mismatch that would have read as "the swap is broken" rather than
"that was never the pair".

**Deliberately not varied from the crate:** hit points stay at 1 and the channel at 1.0s. A barrel that
takes two swings might feel better, but changing two variables at once means an unexpected result has two
possible causes. Tune it once the chain has been watched working here.

**Deliberately not placed in bulk.** One barrel, next to the crate. Dressing the level with these is
worth doing once someone has smashed one and confirmed the broken mesh reads correctly from a player's
eye height rather than from a measurement table.

> 2026-08-18T01:39Z Status restored by claude-grapple: I set this to active by mistake (typed 181 instead of my own 183). It was in review; the evaluated timestamp was re-stamped by the restore. No files were touched.
