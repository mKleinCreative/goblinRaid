---
id: 390
title: Fix grapple rope scaling + add right-click release input
agent: claude-grapple
status: done
claimed: 2026-08-31T15:50Z
build: succeeded
waiting_on:
evaluated: 2026-08-31T21:10Z
observed: 2026-08-31T20:15:22Z | Hook throws and sticks; statue haul completes and topples it; right-click release drops the rope; anchor-distance clamp stops the player at a reasonable range on a plain wall hook; after three fix rounds (rope axis, pivot, segment-length default) the rope renders as one continuous line instead of separate sticks/bars
scenario: Live PIE on L_Tutorial_Island, player character, real playtest by Michael across five rebuild-relaunch cycles
files: 
  - Source/GoblinSiege/Weapons/GSGrappleHaulComponent.h
  - Source/GoblinSiege/Weapons/GSGrappleHaulComponent.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Content/Blueprints/Grapple/BP_GrappleHook.uasset
  - Source/GoblinSiege/Weapons/GSGrappleHookProjectile.h (new)
  - Source/GoblinSiege/Weapons/GSGrappleHookProjectile.cpp (new)
  - Source/GoblinSiege/Weapons/Abilities/GSGA_GrappleThrow.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_GrappleThrow.cpp
  - Source/GoblinSiege/GoblinSiege.Build.cs
---

## Goal

Fix grapple rope scaling + add right-click release input. Scope grew on investigation: #388's own
G/E/R had already found the real cause was not a scaling parameter - `BP_GrappleHook`'s
`HookMesh`/`RopeISM`/`RopeMesh` had the same SCS corruption class that hit `BP_Statue_Warrior`, and
the engine silently drops the corrupted components on every load. Michael separately asked for the
throw to be wired into ACF's Actions System while this was open.

## Generate

1. **`BP_GrappleHook` retired, replaced with a native class** - `AGSGrappleHookProjectile`
   (`Weapons/GSGrappleHookProjectile.h/.cpp`, new). Native `UPROPERTY` subobjects can't suffer the
   SCS-drop corruption at all - there is no SCS tree for the validator to walk. Owns the throw
   (`UProjectileMovementComponent`, same tuned 2600/0.45 values #388 traced the original attach-
   distance spread to), the stick-on-impact behaviour, a visibility-corrected anchor re-trace (per
   `UGSGrappleHaulComponent::NotifyHookAttached`'s own doc on why the raw `Hit.ImpactPoint` sits
   proud of visible art by up to 451uu), and an instanced rope
   (`UInstancedStaticMeshComponent` + `SM_Rope_Segment01`/`MI_GS_Rope`) rebuilt every tick while
   attached.
2. **`UGSGA_GrappleThrow` rebased onto `UACFGameplayAbility`** (was plain `UGameplayAbility`) to wire
   into ACF's Actions System per Michael's request - see `gs-abilities-outside-acf-asc`:
   `CurrentPriority`/combo buffering are armed only by `UACFGameplayAbility::ActivateAbility`/
   `::EndAbility` calling `ACFAbilityComponent->OnAbilityStarted`/`OnAbilityEnded`, which a plain
   `UGameplayAbility` never reaches. Does NOT call `Super::ActivateAbility` (that pipeline commits an
   `ActionConfig` cost against a `UACFGASStatisticsComponent` this character may not have, and would
   silently no-op the whole throw) - calls `OnAbilityStarted` directly instead, keeping the existing
   hand-rolled windup/spawn timing. `Super::EndAbility` (already called, unchanged) reaches
   `UACFGameplayAbility::EndAbility`'s `OnAbilityEnded` call for free, which is what arbitration and
   buffering actually key off. `ActionConfig.bAutoStartCooldown = false` in the constructor keeps the
   unused cooldown half of that same pipeline inert.
3. **`HookProjectileClass` is now a C++ default** (`AGSGrappleHookProjectile::StaticClass()`),
   replacing the `TSoftClassPtr` onto the retired Blueprint - same reasoning as
   `AGSTorchProjectile::FireVolumeClass`.
4. **Added `ActionsSystem` to `GoblinSiege.Build.cs`'s `PublicDependencyModuleNames`** - the exact
   `#166`/`#280` trap the file's own comments already document: `AscentCombatFramework` gave the
   include paths transitively (compiled fine), then the link failed on every `UACFGameplayAbility`
   symbol because the module that actually exports them was never named directly. First LNK2019 catch
   caught it before the ticket closed.
5. **Anchor-distance clamp now applies to ANY attached hook, not just an active haul** - live-playtest
   finding, not part of the original plan: `UGSGrappleHaulComponent::TickComponent`'s rope-distance
   clamp (`ConstrainToRope`, via `MaxRopeStretchUU`) only ever ran while `bHauling` was true. A hook
   stuck in plain scenery (the class header's own "movement tool first" case, and the most common
   throw) enabled no clamp at all, so the rope paid out with no limit. `NotifyHookAttached` now sets
   `HaulAnchorPoint`/`AnchorDistanceAtAttach` and enables tick for every bite, haul or not;
   `TickComponent` runs `ConstrainToRope` unconditionally once a hook is attached and only gates the
   haul-progress/tension logic behind `bHauling`. Tick-disable moved from `EndHaul` to `ReleaseHook`
   (the one true "nothing to hold onto" point) so it doesn't turn off tick a plain anchor still needs.
6. **Rope visual bugs found and fixed across three live-playtest rounds** (each one Michael actually
   watching, not guessed from a compile):
   - Round 1 ("a bunch of sticks planted in the grass"): `SM_Rope_Segment01`'s real bounding box, read
     back live via VibeUE Python, is 7.32 x 7.32 x **50.0** with the long axis on local **Z** - the
     first version assumed local +X, so `Direction.Rotation()` pointed the mesh's true 50uu-long axis
     straight up and its 7.32uu-short axis along the rope. Fixed with
     `FQuat::FindBetweenNormals(FVector::UpVector, Direction)` and swapped scale axes.
   - Round 2 ("still separate bars, but distance is closer" - the distance clamp from item 5 landed in
     the same build): the same live bounding-box read showed the pivot sits at Z **0..50**, not
     centered. Placement had assumed a centered pivot, so scaling stretched each segment from its
     interval's midpoint and left the other half empty - visible gaps. Fixed by placing the pivot at
     each interval's START.
   - Round 3 (still gaps after the axis AND pivot fixes): the actual bug was dumber than either -
     `RopeSegmentLengthUU`'s default was never changed from the original guessed `100.f` to the
     verified `50.f`, so every segment scaled to ~60% of the spacing between them regardless of how
     correctly the axis/pivot logic placed and oriented it. This is the fix that actually closed the
     ticket ("we can close grapple, it looks good!").

## Evaluate

- **Watched live in PIE by Michael, not inferred**: hook throws and sticks, statue haul completes and
  topples the statue, right-click release drops the rope, the anchor-distance clamp stops him at a
  reasonable range on a plain wall hook ("the distance bug is solved"), and after the three rope-visual
  rounds above, the rope itself reads as one continuous line ("it looks good!").
- **Compiled clean three times** (rotation fix, pivot fix, length-default fix), each followed by an
  actual relaunch-and-retest, not just a green build treated as done - `verification-means-runtime-
  not-readback` in project memory is exactly the trap three straight guessed-then-corrected visual bugs
  here would have hidden behind a "builds fine" claim.
- **Not verified**: `HookMeshAsset` (the hook head's own visual) is still unset - the original BP's
  mesh reference could not be read without the editor, which was unavailable when the class was first
  written. The hook is a functional but invisible collision volume in flight, same degrade
  `AGSTorchProjectile` uses for a missing `TorchMesh`. Multiplayer/network behaviour of the ACF
  rebase is unverified - this was a single-player PIE session throughout.
- **AGENT_STATE.md** gets a DECISION entry for the `ActionsSystem` Build.cs dependency (the module
  pattern other agents will hit the moment they call into `ActionsSystem` symbols the same way) and a
  NEXT entry for `HookMeshAsset`.
- Touched outside the original claim: `Source/GoblinSiege/GoblinSiege.Build.cs` (new dependency, item
  4) and the two new `GSGrappleHookProjectile` files - both checked clear via `gsqueue.ps1 check`
  before this ticket closed.

## Refine

- Left deliberately undone: `HookMeshAsset` (see above) - needs an actual mesh assigned in the editor,
  not guessed at in C++.
- Left deliberately undone: no ACF `AbilitySet`/tag migration for the throw - only the base class
  changed (item 2). A full migration to ACF's tag-triggered grant/activate path is a larger, separate
  change and was out of scope for a release-day fix.
- Nothing changed in the anchor-distance-clamp logic (item 5) after the first pass - Michael confirmed
  it fixed on the first retest ("the distance bug is solved").
