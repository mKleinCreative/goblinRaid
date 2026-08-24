---
id: 293
title: Weapon placement: the data asset drives the ACF weapon, and a live console loop to tune it by eye
agent: claude-warren
status: done
claimed: 2026-08-24T22:40Z
build: required
waiting_on:
evaluated: 2026-08-24T23:11:54Z
observed: UNOBSERVED 2026-08-24T23:12:01Z - The loop was driven end to end from script - Set moved the live weapon, Reapply pulled the data asset's value back onto it - but nobody has used it by eye. Its intended first job was Erika's bow, and Michael withdrew that: the bow mesh itself is the wrong asset, so tuning its offset would have proved nothing.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Weapons/GSWeaponDebugCommands.cpp
  - Content/Items/BP_ACFWeapon_ErikaBow.uasset
  - Content/Items/BP_ACFWeapon_ArmingSword.uasset
  - Content/Items/BP_ACFWeapon_GuardSword.uasset
  - Content/Items/BP_ACFWeapon_ScoutPrimary.uasset
  - Content/Items/BP_ACFWeapon_ScoutBow.uasset
  - Content/Items/BP_ACFWeapon_GoblinAxe.uasset
---

## Goal

Michael, having looked at Erika's backwards bow and told me to revert my guess at it:
*"we'll have to come up with a better system for me to manually edit it and for it to work."*

He is right that there was no such system, and the reason is specific. Weapon placement is the one
thing here that **cannot be verified by measurement** - #100 proved a socket check, a
distance-from-origin and a scale read-back all pass on a sword gripped by the point. It is judged by
eye, so the only loop that works is change / look / change again. That loop did not exist.

## Generate

**The weapon data asset is now the single source of truth for placement, on both paths.**

Until now the offset existed **twice**: in `UGSWeaponDataAsset::MeleeMeshOffset`/`RangedMeshOffset`,
and again as a hand-copied duplicate on the static mesh component inside each `BP_ACFWeapon_*`. The
migration plan called that transcription its biggest concrete cost and it was right - **editing the
data asset moved our mesh and did nothing at all to the weapon ACF was holding.** The field that looks
authoritative in the editor silently was not, which is exactly why Michael could not fix his own bow.

`UGSWeaponComponent::ApplyACFWeaponOffsets()` pushes the data asset's offset onto ACF's weapon actor,
called from `RefreshACFWeaponVisibility` - the choke point that already re-asserts the holster rule on
every equipment change, and for the same reason: ACF's `RefreshEquipment` re-attaches its actors and
the attach **snaps** them to the socket, so placement has to be re-asserted on the same beat as the
hide or it is undone by the next replication tick.

`OffsetForWeaponSlot` maps Primary to the melee half and Bow to the ranged half, and returns **null**
for Torch and Grapple rather than identity - identity would silently RESET a weapon rather than leave
it alone.

**Three console commands make it a loop** (`GSWeaponDebugCommands.cpp`), all usable without leaving PIE:

```
GS.Weapon.Dump [filter]      every weapon: which data asset drives it, WHICH FIELD, its offset,
                             and whether ACF or our mesh path is holding it
GS.Weapon.Set <filter> X Y Z Pitch Yaw Roll [Scale]
                             try a value instantly on the live weapon
GS.Weapon.Reapply [filter]   re-read the data assets and push them onto weapons already in hand
```

The intended workflow: `Set` to hunt for the value by eye, type the one you like into the data asset,
`Reapply` to confirm the asset really produces it. **`Set` is deliberately temporary** - it writes to
the live component and never to an asset, so a value typed at a console during a tweak pass cannot
save itself into the project. The value that survives is the one typed into the data asset, which is
also the one `Reapply` reads.

Both commands cover **both paths**, because which one is holding a given weapon depends on a
per-character flag and somebody tuning by eye should not have to know which. `Set` returns the number
of meshes it actually moved, so a command that matched a character but moved nothing reports 0 rather
than looking like it worked.

Resolving the PIE world explicitly rather than trusting the world handed in - the editor's Output Log
console is not the PIE world, and `GS.Raid.*` had to be written twice for exactly that reason.

## Evaluate

**Build succeeded in 1:20**, editor closed, only the two pre-existing `C4996` warnings.

**Every ACF weapon Blueprint was compared against the data asset it should come from BEFORE the
change** - all six matched, so making the data asset authoritative is a pure refactor and cannot move
a weapon Michael has already signed off. Confirmed after the change at runtime, mismatches **0**:

```
CastleGuard01/02   SM_Sword_Arming01  1.637   Knight   GS_Sword_Guard  1.450
ErikaArcher        GS_Bow_Only        1.250   Player   SM_WoodcutterAxe 1.600
HordeGoblin x10    SM_WoodcutterAxe   1.450  (summoned, #291's signed-off value)
```

**The loop itself was exercised, not just compiled:**

```
before Set      relRot yaw=0.0    loc=(10.58,-3.13,-61.43)
after Set 180   relRot yaw=180.0  loc=(10.58,-3.13,-61.43)   <- live, no PIE restart
after Reapply   relRot yaw=-0.0   loc=(10.58,-3.13,-61.43)   <- pulled back from DA_Weapon_Erika
```

That `Reapply` result is the proof the whole ticket rests on: the value returned to the data asset's
value, which means **the data asset is now genuinely driving the weapon ACF is holding.** Before this
change it could not have, because nothing read it.

`GS.Weapon.Dump Erika` names the field to edit by name (`DA_Weapon_Erika` / `RangedMeshOffset`), which
is the other half of the problem - knowing which of several plausible fields is the live one.

**NOT ESTABLISHED: nobody has used this to fix anything yet.** The commands do what they say when
driven from script; whether the loop actually feels usable to the person tuning is Michael's call and
Erika's bow is the test case it was built for.

## Refine

**The offset is applied to the static mesh component, not through ACF's `AttachmentOffset`.**
`AACFWeaponActor::Mesh` is a **skeletal** mesh component and every weapon mesh in this project is
static, so the mesh lives on a component added in the Blueprint that `AlignWeapon` never touches.
`AttachmentOffset` would have been the tidier-looking answer and would have done nothing.

**The mesh component is found as "first static mesh component with a mesh", not by name.** These
actors were all duplicated from one Blueprint and still carry `AxeMesh_GEN_VARIABLE` - a name that is
already wrong on a sword and on a bow, and that a rename would silently break.

**The duplicate values were left in the Blueprints rather than zeroed.** Zeroing would have made the
data asset visibly the only source, but it also means any failure of the new code path puts every
weapon at identity. Leaving them means the Blueprint value is now dead weight that happens to agree -
worth removing once the loop has been used in anger, and noted here so it is not mistaken for the
live value.

**Deliberately not done:** no attempt at Erika's bow. The whole point of the ticket is that the value
is Michael's to choose by eye, and me guessing again is the thing he stopped.

---

### The test case is withdrawn, and the reason matters more than the bow

Michael, asked to try the loop on Erika: *"not at all, and you're using the wrong bow. Stop playing
with Erika and the bow."*

**`GS_Bow_Only` is the wrong mesh.** Every stage of this migration carried that asset forward
faithfully - #289 put it on the player, #292 put it on Erika, and this ticket made a data asset drive
it - and all of that transcription was correct and none of it was the problem. **No amount of offset
tuning would have fixed a bow that is the wrong bow**, which is exactly the failure mode #100 warned
about from the other direction: the measurements agree, and the thing is still wrong.

Recorded here so the next session does not rediscover it by tuning: **do not tune Erika's bow, and do
not assume `GS_Bow_Only` is the intended asset for anybody.** Michael has the right bow in mind and
has not said which it is; that is a question for him, not an investigation.

**What this does NOT invalidate:** the loop itself. `Set` and `Reapply` were exercised end to end and
the data asset demonstrably drives the ACF weapon now, which is the thing that was broken. It simply
has not been used in anger by the person it was built for, and its intended first job turned out to be
the wrong job.
