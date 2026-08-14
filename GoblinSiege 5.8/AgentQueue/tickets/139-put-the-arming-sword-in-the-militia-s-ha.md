---
id: 139
title: Put the arming sword in the militia's hands: DA_Weapon_ArmingSword on the four Militia-row defenders
agent: claude-armingsword
status: done
claimed: 2026-08-12T05:22Z
build: none
waiting_on:
evaluated: 2026-08-12T05:26:49Z
observed: 2026-08-12T05:26:49Z | watched three militia in PIE holding the arming sword by the grip, crossguard above the fist and pommel below, blade angled up and away; endpoint measurement on the live pawn put the grip 23uu from the hand and the tip 230uu away
scenario: PIE on L_CombatArena, GS.Combat.SpawnPatrol 3 0 0, player placed 300uu in front of a BP_CastleGuard01 and screenshotted
files: 
  - Content/Data/Weapons/DA_Weapon_ArmingSword.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard02.uasset
  - Content/Blueprints/Adversaries/BP_PeasantMan.uasset
  - Content/Blueprints/Adversaries/BP_UrielAPlotexia.uasset
---

## Goal

Put the arming sword in the militia's hands: DA_Weapon_ArmingSword on the four Militia-row defenders

Michael, 2026-08-11: *"put the arming sword in the militia's hands"* - the `SM_Sword_Arming01`
imported in #138.

## Generate

### Who "the militia" is, read rather than assumed

`archetype_row_name` on each defender CDO:

| BP | row | weapon before | after |
|---|---|---|---|
| `BP_CastleGuard01` | **Militia** | DA_Weapon_Guard | **DA_Weapon_ArmingSword** |
| `BP_CastleGuard02` | **Militia** | DA_Weapon_Guard | **DA_Weapon_ArmingSword** |
| `BP_PeasantMan` | **Militia** | DA_Weapon_Guard | **DA_Weapon_ArmingSword** |
| `BP_UrielAPlotexia` | **Militia** | DA_Weapon_Guard | **DA_Weapon_ArmingSword** |
| `BP_KnightDPelegrini` | Knight | DA_Weapon_Guard | *unchanged* |
| `BP_ErikaArcher` | Archer | DA_Weapon_Erika | *unchanged* |

**This is why the existing asset could not simply be re-pointed.** All five non-archers share
`DA_Weapon_Guard`, so editing its mesh in place would have re-armed the Knight too - and
`AGENT_STATE` is explicit that a knight is an elite set-piece, not rank-and-file. A new asset was
the only way to hit the four the request names and no others.

### `DA_Weapon_ArmingSword`, built fresh

**Created from the factory, NOT duplicated from `DA_Weapon_Guard`.** `AGENT_STATE` records why:
soft-object mesh fields cannot be cleared from Python (`None` no-ops, `SoftObjectPath('')` throws),
so a duplicated kit drags its bow and quiver along and hangs them on any rig with those sockets -
which is exactly how a floating bow ended up at every guard's feet once before.

Damage, range, turn rate, cooldown, damage tag, attributes effect and granted abilities are all
**copied verbatim** from the guard kit. This ticket swaps a mesh; anything different in those fields
would be an unrequested balance change riding along on an art request.

`melee_socket` stays `hand_r_weapon` - confirmed present on the live pawn (`does_socket_exist` on the
spawned mesh component, which also reports `RightHand` as the bone fallback that #097 used before
#102 authored the socket).

### The scale, derived rather than eyeballed

`GS_Sword_Guard` is 99.8uu long at scale 1.45, i.e. 144.7uu on screen after #099's "a little bigger"
pass. `SM_Sword_Arming01` is 88.4uu, so **1.637** reproduces that same apparent length. Both are then
multiplied by the character mesh scale equally, so the militia's sword now reads at exactly the size
the guard sword was already tuned to rather than at a size I invented.

## Evaluate

### The pivot check that #100 exists to force

Before attaching anything, both meshes were measured along their long axis:

| mesh | geometry along Z | pivot sits |
|---|---|---|
| `SM_Sword_Arming01` | -8.1 .. 80.2 | grip end, 8.1uu of pommel below, blade runs +Z |
| `GS_Sword_Guard` | 0.0 .. 99.8 | grip end (baked there by #103) |

Same convention, so identity rotation was correct and no offset fudge was needed.

Then, on a live PIE pawn, **both endpoints transformed into world space** - because #100 proved that
"0.0uu from the socket" cannot distinguish a sword held by the hilt from one held by the point:

```
grip end  ->  23.2uu from the hand
blade tip -> 229.9uu from the hand
VERDICT: held by the GRIP, tip points away
blade rises 116.3uu above the hand
```

### And then I looked at it

Screenshot from the PIE game viewport, player placed 300uu in front of a `BP_CastleGuard01`: **the
militia are holding the arming sword by the grip**, crossguard sitting just above the fist, pommel
below it, blade angled up and away. Not clipped, not inverted, not floating.

**The one thing the picture shows that the numbers did not: the blade reads as flat white, not
steel.** Its material is a flat-colour `FBXLegacyPhongSurfaceMaterial` instance at
`DiffuseColor 0.80/0.81/0.83` with `Shininess 77` and no roughness, metallic or normal maps, so it
renders bright and slightly plastic next to the armour around it. That is **not a regression and not
a bug in this ticket** - it is the state #138 imported it in, and `substance_material_plan.md` is the
authored fix that does not exist yet. Flagging it because "the sword is in his hand" and "the sword
looks right" are different claims and only the first one is delivered here.

**Not verified:** nobody has swung it. The mesh is a visual prop - damage comes from
`FGSSwingStage`, and `UGSWeaponDataAsset::AttackCooldownSeconds` is documented as read by nothing -
so a swing should be unaffected, but "should" is doing work in that sentence and no fight has been
watched with the new asset equipped.

**Owed to `AGENT_STATE.md`:** militia and knights no longer share a weapon asset. Anyone re-pointing
`DA_Weapon_Guard` in future is now changing the Knight and Erika's melee only.

## Refine

**Changed in response to my own review:** the first plan was to re-point `DA_Weapon_Guard` at the new
mesh - one edit instead of five. Reading the archetype rows killed it: that asset is worn by the
Knight as well, and re-arming an elite set-piece was not what was asked for.

**Cleanup:** `L_CombatArena.umap` was left dirty by the temporary actors used for the capture. The
actors were destroyed and the map reverted with `git checkout`, since no intentional level change was
made. Verified zero `ZZTEMP`/`ZZPROBE` actors remain.

**Deliberately left undone:**

- **No holster/back-carry.** `MeleeHolsterSocket` defaults to `back_sword`, which
  `does_socket_exist` reports as **absent** on the human rig - so a holstered pose would attach to
  nothing. Left alone rather than papered over; it is the same missing-socket gap #097 recorded.
- **The blade's shading.** Fixing it properly means authoring the Substance texture set; fixing it
  cheaply means inventing colour values that contradict the artist's own validated Blender preview.
  Neither belongs in a ticket whose request was to put the sword in a hand.
- **The Knight keeps `DA_Weapon_Guard`.** If he should also carry the arming sword, that is one line
  and a word from Michael.
