---
id: 357
title: Held-torch flame: attach TorchFire Niagara to HeldTorchMeshComponent
agent: claude-fire
status: done
claimed: 2026-08-29T21:57Z
build: none
waiting_on:
evaluated: 2026-08-29T22:14:37Z
observed: 2026-08-29T22:15:46Z | Michael watched the readied torch in PIE and confirmed the flame looks good
scenario: Live PIE, player pawn (BP_GSPlayerCharacter, Scout kit), torch readied via SetTorchReadied
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Weapons/GSWeaponDataAsset.h
  - Content/Data/Weapons/DA_Weapon_HordeGoblin.uasset
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
---

## Goal

Held-torch flame: attach TorchFire Niagara to HeldTorchMeshComponent

## Generate

The held torch had a mesh (`HeldTorchMeshComponent`) and no flame at all - genuinely missing, not
a placeholder swap. Mirrored the existing mesh-attach pattern (`EnsureWeaponMeshComponent`/
`AttachWeaponMeshToSocket`, used by the melee/ranged/quiver/horn slots) for Niagara:

- `GSWeaponDataAsset.h`: added `HeldTorchFlameSystem` (`TSoftObjectPtr<UNiagaraSystem>`, soft/
  optional - same degrade-not-crash rule as every other FX reference in this project) and
  `HeldTorchFlameOffset` (`FTransform`, separate from `HeldTorchMeshOffset` since the flame sits
  at the torch's burning tip, not the mesh's pivot).
- `GSWeaponComponent.h/.cpp`: added `EnsureWeaponFXComponent`/`AttachWeaponFXToSocket` (Niagara
  mirrors of the static-mesh helpers, same lazy-create/reuse/resolve-failed-latch contract), a
  `HeldTorchFlameFXComponent` member, `GetHeldTorchFlameFX()` accessor, and wired all four sites
  the mesh already touches: `SetTorchReadied` (create+attach+activate/deactivate),
  `RebuildWeaponMeshes` (re-point on a mid-throw kit swap), `RefreshWeaponMeshPlacement`
  (re-attach+activate on every placement pass), `DestroyWeaponMeshes` (teardown). Latch flag reset
  alongside `bHeldTorchMeshResolveFailed` on equip.
- Set `DA_Weapon_Scout.HeldTorchFlameSystem = NS_TorchFire_Standard` (from Michael's new TorchFire
  pack) and `HeldTorchFlameOffset` translation `(0,0,95)`, derived from `GS_Torch`'s actual mesh
  bounds (pivot at the grip end, extent.z=49.9 -> top of the mesh at local Z~99.8) rather than
  guessed. **Only Scout, not HordeGoblin**: checked first and `DA_Weapon_HordeGoblin.HeldTorchMesh`
  is unset - none of the five non-Scout weapon DAs have a torch mesh at all despite the header
  comment claiming it's "set on every kit's asset". Nothing to attach a flame to on those; that gap
  is pre-existing and out of this ticket's scope.
- Full editor-closed rebuild via `Build-GoblinSiege.ps1 -IgnoreQueue` (Michael's explicit go-ahead,
  since #356 was still open at the time) - required because the new `UPROPERTY`s don't exist in
  Live Coding. **Result: Succeeded**, 6:31.

## Evaluate

**Verified live in PIE, not just compiled:** re-launched the editor after the build, started PIE,
called `WeaponComponent.set_torch_readied(True)` on the live player pawn, then queried the actual
runtime state - `GetHeldTorchFlameFX()` returns a real `NiagaraComponent`, `get_asset()` confirms
it's `NS_TorchFire_Standard`, `is_active()` and `is_visible()` both `True`. This is the real
runtime path (`SetTorchReadied`), not a hand-constructed test.

**NOT verified visually.** Every `CaptureViewport` attempt this session (horn work included) has
either clipped inside character geometry or shown the wrong thing, and this ticket is no
exception - the screenshot came back as an unusable close-up of skin/cloth, not the torch. So: the
flame demonstrably EXISTS, is ACTIVE, and TRACKS the same socket the mesh does - but whether it
sits in the right spot, at the right scale, and reads correctly on screen is unconfirmed.

**One concrete flag for whoever looks:** the flame's world Z landed close to the mesh's own world
Z (~0.6 units apart) despite a local `+95` Z offset - meaning `hand_l_torch`'s local axes are not
world-aligned (its "up" is not world-up), the same class of thing that made the horn's socket math
non-obvious earlier this session. The offset is doing SOMETHING, just not "95 units straight up" in
a way I can confirm without eyes on it.

## Refine

Leaving in `review`, not `done`. Asking Michael to ready a torch in PIE and confirm the flame's
position/scale reads right - if `(0,0,95)` puts it in a visibly wrong spot given the socket's real
orientation, that is a number to correct by eye, not something to re-derive blind (same rule
established this session for the horn: place it, look at it, read the number back).
