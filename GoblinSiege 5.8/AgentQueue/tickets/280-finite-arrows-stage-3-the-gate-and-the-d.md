---
id: 280
title: Finite arrows stage 3 - the gate and the decrement
agent: claude-warren
status: done
claimed: 2026-08-24T04:12Z
build: required
waiting_on:
evaluated: 2026-08-24T04:28:10Z
observed: 2026-08-24T04:28:11Z | The bow now spends an arrow a shot - thirty went to twenty-nine on one loose - and an empty quiver simply does not fire: no draw, no arrow in the world. One accepted press plus two rate-limited ones cost exactly one arrow, not three. Erika, holding zero arrows, shot anyway, which is the whole of ruling 48.
scenario: PIE on L_CombatArena after an editor-closed rebuild, firing GSGA_BowShot on the live player and on BP_ErikaArcher.
files: 
  - Source/GoblinSiege/GoblinSiege.Build.cs
  - Source/GoblinSiege/Weapons/GSWeaponDataAsset.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
---

## Goal

Stage 3 of the finite-arrows plan (rulings 46 and 48, #277). The bow spends an arrow, refuses when
empty, and **AI archers are untouched**.

## Generate

**`GoblinSiege.Build.cs`** - `"InventorySystem"` added. **Verified necessary rather than assumed.**
The file contains two contradictory paragraphs: an older one saying the ACF modules arrive
transitively through `AIFramework` and that listing them "would be noise", and #166's correction
immediately below it - *"A transitive dependency gives you the INCLUDE PATHS ... and then the link
fails with LNK2019 on every ACF symbol you actually called. Add the next ACF module here the moment
you CALL into it."* Nothing in our C++ called `InventorySystem` before today, so this is the first
call. The new comment names what we reach for, matching the file's style.

**`UGSWeaponDataAsset::ArrowItemClass`** - a `TSubclassOf<UACFItem>` in the existing `RangedMode`
block beside `RangedAttackCooldownSeconds`, with the same `EditCondition = "bHasRangedMode"` as its
neighbours.

**On the data asset, not the ability.** There is no `GA_GS_BowShot` Blueprint, so a property on
`UGSGA_BowShot` would only ever hold its C++ default and reaching it would need either a
`ConstructorHelpers` path into content or a new Blueprint plus a change to the ability grant. The
ability already reads this asset for the fire interval, and "which ammo" is a property of the bow.

**`UGSGA_BowShot::GetAmmoItemClass(ActorInfo)`** - one helper, two call sites, returning the class
this shot must spend or **null meaning free**. Non-null only when BOTH: the avatar has a
`UGSBowTimingComponent`, and the equipped weapon has a non-null `ArrowItemClass`.

- **Refusal** in `CanActivateAbility`, after the rate-limit block. A refusal there costs nothing - no
  `CommitAbility`, no instance, no wind-up task - and held-fire simply stops rather than stuttering.
- **Consume** in `FireArrow` at the END of the existing `if (Arrow)` block, inside the existing
  `HasAuthority()` guard. `ConsumeItems({FBaseItem(AmmoClass, 1)})`.

### Why the consume is last, and why that is not cosmetic

The draw-quality read - `Arrow->SetDrawQualityMultiplier(Timing->GetLastReleaseQuality())` - sits at
the top of that same block. A consume placed **earlier** with an early return on failure would skip
it and silently give **every player arrow the default 1.0 multiplier**. The bow timing minigame would
become decoration and *nothing would report it*. Placing the consume after the spawn also means a
spawn that failed cannot eat an arrow.

### Keeping AI archers free (ruling 48)

`UGSGA_BowShot` is shared: `BP_ErikaArcher`'s `RangedAttackAbilityClass` and the player's
`BowShotAbilityClass` are the same class with no Blueprint child between them, so a naive count check
would leave an archer in a **permanent draw** the moment she ran out.

Test 1 is the discriminator this file already uses twice and documents at `:186-195` - *"an AI archer
simply has nothing to ask... There is no condition here to get wrong later."* Deliberately **not**
`IsPlayerControlled()`, which flips the first time anyone possesses an archer for a debug session.
Test 2 means `DA_Weapon_Erika` staying null is a second, independent reason she pays nothing.

**Both tests fail OPEN**: a mis-authored asset costs infinite arrows, never a dead bow. Same doctrine
as `TorchProjectileClass`'s C++ default.

**The residual hazard is diagnosed, not gated.** If anyone ever gives an AI archer a
`UGSBowTimingComponent`, test 1 flips and that archer silently acquires a quiver it cannot refill.
`GetAmmoItemClass` logs once, naming ruling 48 and both fixes, when it returns non-null for a pawn
with no `APlayerController`. A latch, copied from `UGSGA_TorchToss`'s montage warning - a diagnostic
rather than a second gate, so the gate stays structural.

## Evaluate

**Build succeeded in 1:30**, editor closed. **No LNK2019**, which is the evidence the Build.cs change
was both necessary and sufficient. Only the two pre-existing `C4996 AbilityTags` warnings.

**Runtime, PIE on `L_CombatArena`:**

| check | result |
|---|---|
| one shot spends one arrow | 30 -> **29**, one arrow actor in the world |
| one accepted press + two rate-limited presses | 45 -> **44** - exactly one spent |
| empty quiver refuses | `activate -> False`, and **zero** arrow actors spawned |
| Erika with **zero** arrows | `activate -> True` - she is unaffected |

**I got the rate-limit test wrong the first time and it is worth recording.** The first attempt fired,
immediately re-fired, read the count, saw it unchanged and concluded "refused presses cost nothing".
That was **measurement error**: the activation had returned `True`, so the press was not refused at
all - the count simply had not moved yet, because `FireArrow` runs after the 0.08s release delay. The
real test fires three times inside one script (microseconds apart, so the second and third are
genuinely rate-limited) and reads the count on a later round trip: `True, False, False` and 45 -> 44.
**A count that has not changed yet looks exactly like a count that will not change.**

**Erika's test is decisive rather than suggestive.** She holds **zero** arrows. If the gate applied to
her at all, an empty inventory would refuse her instantly. She fired.

**The tuned ranged work is untouched, and that is checkable rather than argued:** `git status` shows
`GSArrowProjectile.{h,cpp}`, `GSAimComponent.cpp` and `GSBowTimingComponent.cpp` are **not modified**.
The derived arrow-mesh offset, the distance-to-head-bone headshot and the draw minigame cannot have
moved. The draw-quality line was additionally confirmed to precede the consume in the same block, and
since the count decremented, that block ran.

**Not established:** that a good release still out-damages a poor one *by observation*. The argument
is structural (same block, consume strictly after) and the code was read back to confirm the order,
but nobody has drawn the bow by hand and compared damage numbers. Also not established: the
client-side count lag, which is expected and documented rather than tested.

## Refine

**No `CostGameplayEffectClass`**, though it is the GAS-idiomatic answer. The count lives in a
replicated FastArray, not an attribute set, so a cost GE would need a mirror attribute kept in step
with the inventory - a second copy of the truth. That is exactly what this ability's own header argues
against for the fire interval, and what `UGSAimComponent` was created to stop for the muzzle transform.

**Client count lag is left alone deliberately.** `CanActivateAbility` runs client-side against a
possibly-stale replicated count; the server's `FireArrow` is the real gate. A client predicting a shot
the server refuses gets no arrow - degraded, not broken. Moving the consume client-side to close that
window would break authority, and the comment in the code says so.

**Deliberately not done:** the HUD count is Stage 4 and needs its own build. Until it lands the player
has no way to see how many arrows they have, which makes running dry a surprise rather than a
decision - so **stage 4 is not optional polish**, it is the half that makes this playable.
