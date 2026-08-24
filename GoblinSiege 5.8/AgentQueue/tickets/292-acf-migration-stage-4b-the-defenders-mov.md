---
id: 292
title: ACF migration stage 4b: the defenders move onto ACF
agent: claude-warren
status: done
claimed: 2026-08-24T22:01Z
build: required
waiting_on: Michael: LOOK AT THE WEAPONS. PIE is running on L_CombatArena. Three checks in Evaluate.
evaluated: 2026-08-24T22:36:20Z
observed: 2026-08-24T22:35:17Z | Michael played it: the castle guards stand holding their arming swords in the right hand at the size they have always been, the Knight - placed in this level for the first time - holds his guard sword correctly with no sign of the tip-pivot problem #100 documented, and Erika now comes up with the bow in her hand instead of on her back. ACF is holding every one of those weapons instead of our mesh path. Her bow points the wrong way round, which it did before the migration too and is not something this work changed.
scenario: Michael playing in PIE on L_CombatArena with all four defender types placed - two CastleGuard01, two CastleGuard02, two ErikaArcher and the newly placed Patrol_Knight_1.
files: 
  - Content/Items/BP_Item_ArmingSword.uasset
  - Content/Items/BP_ACFWeapon_ArmingSword.uasset
  - Content/Items/BP_Item_GuardSword.uasset
  - Content/Items/BP_ACFWeapon_GuardSword.uasset
  - Content/Items/BP_Item_ErikaBow.uasset
  - Content/Items/BP_ACFWeapon_ErikaBow.uasset
  - Content/Data/Characters/DA_Char_Militia.uasset
  - Content/Data/Characters/DA_Char_Knight.uasset
  - Content/Data/Characters/DA_Char_Archer.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard02.uasset
  - Content/Blueprints/Adversaries/BP_KnightDPelegrini.uasset
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
  - Config/DefaultGameplayTags.ini
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Michael: *"let's move the defenders onto ACF because they're going to have to use that system anyways
for patrols and the like."*

Four defenders: **CastleGuard01, CastleGuard02, KnightDPelegrini, ErikaArcher**. Not the peasant (a
civilian carrying a basket, #285) and not Uriel (ruling 55 puts him in the public demo, not the
prototype).

## Generate

Three weapons authored, each carrying its measured transform **verbatim** rather than tidied:

| item | mesh | scale | translation | slot | type |
|---|---|---|---|---|---|
| `BP_Item_ArmingSword` | `SM_Sword_Arming01` | 1.637 | 0,0,0 | RightHand | `WeaponType.Sword` |
| `BP_Item_GuardSword` | `GS_Sword_Guard` | 1.45 | 0,0,0 | RightHand | `WeaponType.Sword` |
| `BP_Item_ErikaBow` | `GS_Bow_Only` | 1.25 | **10.578, -3.125, -61.426** | LeftHand | `WeaponType.Bow` |

**Erika's translation is the reason this is transcription and not authoring.** `AttachmentOffset` only
moves ACF's own skeletal `Mesh`; our mesh is static and lives on a component `AlignWeapon` never
touches, so a scale-only copy would have put her bow **61uu** from where it has always been. Copied
onto the component by hand, like #287 and #289.

`GS_Sword_Guard` has the **tip pivot** #100 documented, and `DA_Weapon_Guard`'s translation is
nevertheless `0,0,0`. That is carried across unchanged. It may well be wrong, but it is wrong exactly
as it is today, and a migration is the wrong place to fix it - changing it here would make a placement
regression and a placement improvement indistinguishable.

**Collision was already right and was checked anyway.** All three duplicate `BP_ACFWeapon_ScoutPrimary`,
which #287 fixed to `NoCollision` after the camera lurch - so the fix propagated. Verified rather than
assumed, because four defenders stand near the player and #145's crowd lurch is exactly this bug at
scale.

`DA_Char_Militia` (both guards), `DA_Char_Knight` and `DA_Char_Archer` each gain one starting item with
**`AutoEquip = true`**. The reverse-reference check confirmed each data asset is used by exactly the
characters expected - the militia asset by the two guards and nothing else.

## Evaluate

**Runtime, PIE on `L_CombatArena`.** Four castle guards spawned, each with
`MainWeapon = BP_ACFWeapon_ArmingSword` - ACF holding the sword, not our mesh path. The Knight is not
placed in this level, so nothing about him has been seen run.

**Erika came up with her bow ON HER BACK, and the cause is a genuine defect the guards were hiding.**

`UACFEquipmentComponent::UseEquippedItemBySlot` opens with:

```cpp
if (ItemSlot == CurrentlyEquippedSlotType) { SheathCurrentWeapon(); return; }
```

**The call is a toggle, not an assignment.** Demonstrated at runtime on the live pawn - three calls on
the same slot gave `drawn -> None -> drawn`.

So the number of syncs during startup decides whether a defender ends up armed:

- **Guards**: `DefaultSlot` is `Primary`, which already equals the component's `CurrentSlot`, so
  `SetSlot` refuses the change before reaching ACF. **One** sync - `BeginPlay`'s next-tick timer.
  Drawn.
- **Erika**: `DefaultSlot` is `Bow`. `SetSlot(Bow)` is a real change, so it syncs and draws - and then
  the next-tick timer fires and syncs the **same** slot again, which ACF reads as sheathe. **Two**
  syncs. Sheathed.

Identical code, identical data, opposite results, decided by parity. This has been latent since #287
put the next-tick timer in; the player never hit it because his slot also starts at `Primary`.

**Fix:** `SyncACFEquippedSlot` returns early when ACF's drawn main weapon is already the actor equipped
in the target slot. Compared by actor because `CurrentlyEquippedSlotType` is private with no accessor.
This makes the sync idempotent for every caller, not just the timer.

### After the fix, built and re-run

**Build succeeded in 19s**, editor closed, only the two pre-existing `C4996` warnings.

**Every defender now comes up drawn, including both Erikas** - the case that was broken:

```
BP_CastleGuard01 x2   slot=WeaponSlot.Primary   MainWeapon=BP_ACFWeapon_ArmingSword
BP_CastleGuard02 x2   slot=WeaponSlot.Primary   MainWeapon=BP_ACFWeapon_ArmingSword
BP_ErikaArcher   x2   slot=WeaponSlot.Bow       MainWeapon=BP_ACFWeapon_ErikaBow
BP_GSPlayerCharacter  slot=WeaponSlot.Primary   MainWeapon=BP_ACFWeapon_ScoutPrimary
```

The player is in that list deliberately: the fix touches the shared sync path, so #287's and #289's
result had to be re-confirmed rather than assumed still true.

**Placement matches the pre-migration measurements exactly**, which is the check the ledger says this
migration passes or fails on:

```
SM_Sword_Arming01  hand_r_weapon  scale 1.637  relLoc (0,0,0)             NoCollision
GS_Bow_Only        hand_l_weapon  scale 1.250  relLoc (10.578,-3.125,-61.426)  NoCollision
```

**No duplicate weapons.** Our mesh path built nothing on any of the three - `IsSlotOwnedByACF`
correctly stood it down, so nobody is carrying two swords.

**LOOKED AT AND SIGNED OFF.** Michael played it: both castle guards hold their arming swords in the
right hand at the size they have always been, the Knight holds his guard sword correctly, and Erika
comes up with the bow in her hand rather than on her back - the case that was broken. The player is
unchanged from stage 2, so the shared sync fix broke nothing that was already working.

**One fault found by eye that no number above could see: Erika's bow points the wrong way round.**
It is not a regression and it is not fixed here - both are established below.

## Refine

**The defect this stage found is worth more than the migration.** `UseEquippedItemBySlot` being a
toggle means *any* future caller that syncs twice silently un-draws the weapon, and the symptom is a
character standing there unarmed with every log line clean and every property correct. It had been
latent since #287 and only surfaced because Erika is the first character whose `DefaultSlot` differs
from the component's default - the guards' matching slot is what hid it.

**Fixed at the sync rather than at the timer**, deliberately. Making `BeginPlay`'s next-tick timer
conditional would have fixed Erika and left the trap armed for the next caller. An idempotent
`SyncACFEquippedSlot` cannot double-fire from anywhere.

**Compared by weapon ACTOR, not by slot tag**, because `CurrentlyEquippedSlotType` is private with no
accessor. Same fact, public API, no plugin edit.

**Deliberately not done:** `GS_Sword_Guard`'s tip pivot is carried across unchanged rather than
corrected. Fixing it inside a migration would make a placement regression and a placement improvement
impossible to tell apart - it wants its own ticket, against the Knight, in a level he is actually in.

**Also found, not fixed:** `BP_UrielAPlotexia` has **no `CharacterInitDataAsset` at all** - the same
defect #275 found on the peasant, so his ACF stats never initialise. Ruling 55 puts him in the public
demo rather than the prototype, so this is recorded for whoever picks that up rather than fixed here.

> 2026-08-24T22:19Z Built and verified at runtime. Needs Michael's eyes on the two guard swords and Erika's bow.

---

### The Knight, placed

Michael: *"can we just go ahead and place the knight in the map anyways?"* `Patrol_Knight_1` at
**(600, 0, 130) yaw 180**, the free spot in the militia line (they sit on x 440-760, y 0/139, archers
on y -139). He holds `BP_ACFWeapon_GuardSword` correctly - **the tip pivot #100 documented is not
visible on him**, so carrying `DA_Weapon_Guard`'s `0,0,0` translation across verbatim was right, and
there is nothing here to fix after all.

### Erika's bow points the wrong way, and it is NOT this migration

Worth recording carefully, because it looks exactly like a migration regression and is not one.

Both paths were measured on the same pawn, one PIE run each, comparing the mesh's two local endpoints
in world space (#100's rule - an origin distance says nothing about which way a mesh faces):

```
ours (flag off): A (566.00, -33.97,  1.07)   B (606.37, -138.02, -50.27)   rot (-29.6,  36.4, -118.7)
ACF  (flag on):  A (572.83, -23.65,  7.42)   B (608.14, -127.99, -46.97)   rot (-28.4,  34.2, -120.2)
```

~14uu and 1-2 degrees apart, which is her idle animation being at a different frame between two runs,
not a systematic difference. `DA_Weapon_Erika.RangedMeshOffset` carries an **identity rotation**, and
the ACF component carries the same - nothing was dropped in transcription. **The bow has been backwards
all along.**

A 180-degree flip about the bow's own long axis was tried and **reverted at Michael's instruction**:
the tips moved less than 3uu as predicted, so the transform did what it claimed, but he does not want
the value guessed at from here. **His call:** *"we'll have to come up with a better system for me to
manually edit it and for it to work."* That is a real gap - weapon placement is judged by eye and is
currently only reachable by an agent editing a component property between PIE runs. It wants its own
ticket, and it is not this one.

`BP_ACFWeapon_ErikaBow` is back to `rot (0,0,0)`, verified by re-reading the saved asset, and matches
`DA_Weapon_Erika` exactly. Both paths therefore still agree, wrongly, which is the state to preserve
until somebody can author the right value and see it.
