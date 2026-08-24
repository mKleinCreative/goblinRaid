---
id: 289
title: "ACF wheel migration stage 3: the bow moves, the quiver does not"
agent: claude-warren
status: review
claimed: 2026-08-24T21:50Z
build: required
waiting_on: "Michael: LOOK AT THE BOW. Every measurement matches, but placement is invisible to a socket check. PIE is running with the bow drawn. Four things in Evaluate."
evaluated: 2026-08-24T21:35:16Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Content/Items/BP_Item_ScoutBow.uasset
  - Content/Items/BP_ACFWeapon_ScoutBow.uasset
  - Content/Data/Characters/DA_Char_Player.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Config/DefaultGameplayTags.ini
---

## Goal

Stage 3 of ruling 53. The bow's ranged half moves onto ACF. **The quiver does not, and must not.**

## Generate

`BP_ACFWeapon_ScoutBow` + `BP_Item_ScoutBow`, `GS_Bow_Only` at **scale 1.25**, sockets
`hand_l_weapon` / `back_bow` - the same two `DA_Weapon_Scout` has always used. New tag
`WeaponType.Bow`, added to the player's `AllowedWeaponTypes` beside `WeaponType.Axe`.

**`HandleType = ONE_HANDED`, deliberately not `OFF_HAND`**, even though the bow lives in the off hand.
ACF's off-hand branch in `UseEquippedItemBySlot` only fires when a one-handed main weapon is already
drawn, and on that path it sets **neither `MainWeapon` nor `CurrentlyEquippedSlotType`** - the bow
would attach to the hand and never count as drawn, which is the sort of half-state that produces a
weapon you can see but cannot use.

**Collision set to NoCollision up front**, applying #287's lesson before it could bite rather than
after. That ticket's camera lurch was a held weapon blocking `ECC_Camera`; a bow held across the chest
is at least as good a candidate.

**`RefreshACFWeaponVisibility` now covers both slots, with deliberately DIFFERENT rules**, each
mirroring its own branch of `RefreshWeaponMeshPlacement`:

- **Primary** keeps the `!bAimActive && !bRangedActive` terms - the axe is hidden for the whole time
  the bow is out, because it sits on `back_sword` right where the over-the-shoulder camera looks.
- **Bow** has neither term. It hangs on `back_bow`, behind the shoulder rather than across the camera
  line, so it never had the problem the axe has and is simply shown whenever the weapon says so.

Copying the axe's rule onto the bow would have hidden the bow every time the bow was out.

**Our ranged mesh is suppressed once ACF owns the slot**, same as the melee in #287 - otherwise two
bows on one socket. **The quiver is explicitly excluded from that**, and the comment says why: ACF's
`RefreshEquipment` re-attaches and re-shows every equipped item on each equipment change, which would
break *"the quiver never moves and is never hidden"* outright.

## Evaluate

**Build succeeded in 16s.** Runtime, PIE on `L_CombatArena`:

```
SPAWN     axe  hand_r_weapon  shown      bow  back_bow      shown     quiver spine_quiver OURS shown
BOW OUT   axe  back_sword     HIDDEN     bow  hand_l_weapon shown     quiver spine_quiver OURS shown
```

Identical to the baseline #286 measured before any weapon moved. **Neither ACF weapon blocks
`ECC_Camera`.**

**The ranged systems that hang off the bow all survive**, which was the real risk of moving it:

- the bow **fires** and an arrow spawns;
- the quiver **still spends** - 15 arrows to 14;
- the tuned arrow offset is **intact**: `pitch -90`, `X = -74.375`, `scale 1.25`. That X is the
  *derived* value (`-59.5 x 1.25`) the plan flagged as impossible to transcribe safely, and it is
  still exactly right;
- the bow timing component and the aim component are both still present on the pawn.

**NOT ESTABLISHED, and it is why this stage stops here: nobody has looked at the bow.** Every check
confirms the right socket, owner and visibility flag; none of them can see whether the bow is the
right size, the right way round in the hand, or clipping the quiver. **PIE is running with the bow
drawn.** Four things:

1. **The bow in hand** - same size and grip as before, one bow not two.
2. **The quiver** - unmoved on the back, and still there through every swap.
3. **Swap to primary** - the bow goes to `back_bow` and stays visible; the axe comes to hand.
4. **Draw and loose** - the aim arc starts where it always did and the arrow flies nose-first.

## Refine

**A behaviour change from #287 worth stating rather than leaving to be discovered.** With the TORCH
selected, our old mesh path treated the melee weapon as "active" and kept the axe **in hand** beside
the torch. ACF sheathes it instead, so the axe now goes to the back when the torch comes out. Michael
saw and accepted this while closing #287 - it was check 4 in that ticket - but it was a consequence of
the migration rather than a decision, and it is recorded here so nobody later reads it as a bug.

**The two visibility rules were kept separate rather than unified.** They look similar enough to merge
and are not: the melee rule exists because of where `back_sword` sits relative to the camera, and the
bow has no such problem. A single shared rule would have to encode which sockets are in shot, which is
more cleverness than two four-line branches are worth.

**Deliberately not done:** the quiver, the horn and the torch stay on our mesh path. Stage 5 retires
whatever of that path is provably dead, and all three of those keep it alive.
