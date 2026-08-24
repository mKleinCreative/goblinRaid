---
id: 275
title: Peasants become civilians - no sword, 10 HP, and an ACF data asset they never had
agent: claude-warren
status: done
claimed: 2026-08-24T02:58Z
build: none
waiting_on: "Michael: the project has NO bucket mesh. Pick a stand-in (SM_Pot / SM_Basket / SM_Jug) or say a bucket should be sourced. The bucket brigade behaviour is a separate piece of work - see Refine."
evaluated: 2026-08-24T03:10:27Z
observed: 2026-08-24T03:10:28Z | The peasant now stands in the level carrying the basket in his right hand instead of an arming sword, and reads 10 health on both ACF and our attribute set with no mismatch. Before this he was a 30 health militia swinging a sword, and his ACF statistics had never initialised at all.
scenario: PIE on L_Tutorial_Island, the only map BP_PeasantMan is placed in.
files: 
  - Content/AI/DA_Race_Human.uasset
  - Content/Blueprints/Adversaries/BP_PeasantMan.uasset
  - Content/Data/DT_GSAttributeInits.uasset
  - Content/Data/Characters/DA_Char_Civilian.uasset
  - Content/Data/Weapons/DA_Weapon_Bucket.uasset
---

## Goal

Michael, 2026-08-23: *"Remove sword, give them a bucket and make them civilians for the bucket
brigade."* `BP_PeasantMan` was archetype `Militia` carrying `DA_Weapon_ArmingSword` - a peasant with
30 HP who fought back - while GDD 8 says *"civilians 10 HP with disbelief -> panic"*.

## Generate

**Civilians did not exist as a concept.** `DA_Race_Human` had exactly three archetype rows - Militia,
Archer, Knight - and no civilian row at all, which is why the peasant was a militia in the first
place. There was nothing else to be.

Changed, all data, no build:

1. **`DA_Race_Human` gains a `Civilian` archetype row** - 10 HP, armour 0, **damage 0**, attack range
   0, `BehaviorTree` deliberately unset. Built by copying the Militia row's text and overriding only
   what makes a civilian a civilian, so every unrelated field keeps an authored value rather than a
   Python default.
2. **`DT_GSAttributeInits` gains a `Civilian` row** - Health and MaxHealth 10, Strength 1. This is
   ACF's side of the same numbers; see below for why it was needed.
3. **`DA_Char_Civilian`** created by duplicating `DA_Char_Militia`, pointed at the new table row,
   renamed, and carrying `DA_AbilitySet_GS_Empty` (inherited from the duplicate, and correct per #273).
4. **`BP_PeasantMan`** - `ArchetypeRowName` Militia -> **Civilian**, `DefaultWeapon`
   `DA_Weapon_ArmingSword` -> **None**, and its ACF initializer's `CharacterInitDataAsset` None ->
   `DA_Char_Civilian`.

**Removing the sword is safe because the archetype carries the behaviour tree.** The worry was
leaving a militia-brained NPC swinging at people with empty hands. It cannot happen: the BT comes
from `FGSArchetypeDefinition::BehaviorTree`, and `AGSAIControllerBase.cpp:144` guards it
(`if (UBehaviorTree* BT = ...LoadSynchronous())`). A Civilian row with no tree means no tree runs -
the peasant simply stands there until `BT_Civilian` exists. Inert, not broken.

`BP_PeasantMan` is placed only in `L_Tutorial_Island`, so none of this disturbs the combat arena.

## A pre-existing defect found by running it

The first runtime check produced **`BP_PeasantMan ARS 0/0 hp | GS 10/10 hp <-- MaxHealth MISMATCH`**
- the only mismatch in the level. `BP_PeasantMan`'s `ACFCharacterInitializerComponent` had **no
character data asset at all**, so ACF never initialised its statistics. Every other character in the
project has one.

**This was not caused by this ticket** - the GS-side change touches the archetype row and
`DefaultWeapon`, neither of which feeds ACF. It was already mismatched at 30 HP; nobody had ever run
`GS.Stats.Dump` on `L_Tutorial_Island`. Since #228 `AGSCharacterBase::GetHealth()` reads ARS first,
so a character with an uninitialised ARS is a live hazard, not a cosmetic one.

**I got the fix wrong on the first attempt, and the runtime caught it.** The component has two
similar properties and I set the wrong one:

- `CharacterDataAsset` - `ReplicatedUsing=OnRep_CharacterDataAsset, BlueprintReadOnly`, the **runtime**
  value (`ACFCharacterInitializerComponent.h:117`)
- `CharacterInitDataAsset` - `EditAnywhere`, the **authored input** that actually drives init (`:106`)

Setting the first changed what the pawn reported while changing nothing about its initialisation -
ARS stayed 0/0 through a full PIE cycle. Only setting `CharacterInitDataAsset` fixed it. **Worth
remembering: on this component the property you can read back is not the property that does the
work**, and reading it back would have "confirmed" a fix that had done nothing.

## Evaluate

Runtime, PIE on `L_Tutorial_Island` where the peasant is actually placed:

```
BP_CastleGuard01_C_0   ARS 30/30 hp  |  GS 30/30 hp  armor 0
BP_CastleGuard02_C_0   ARS 30/30 hp  |  GS 30/30 hp  armor 0
BP_PeasantMan_C_0      ARS 10/10 hp  |  GS 10/10 hp  armor 0
BP_GSPlayerCharacter_C_0  ARS 100/100 hp | GS 100/100 hp
GS.Stats.Dump: 4 character(s), 0 MaxHealth mismatch(es).
```

Was `1 MaxHealth mismatch` before. The peasant also reports `EquippedWeapon=NONE` at runtime - the
sword is genuinely gone, not just cleared on the CDO.

**Established:** the health figures on both sides at runtime; that the sword is gone; that the data
table refill did not drop any of the six pre-existing rows (checked explicitly before and after,
because `fill_data_table_from_json_string` **empties the table** and a silent row loss would have
been catastrophic and invisible).

**Not established:** that a civilian with no behaviour tree behaves acceptably in a real raid. It was
observed standing still, which is what the code says should happen, but nobody has watched a goblin
attack one.

## Refine

**Two thirds of the instruction are done. The bucket is not, and I am not going to guess at it.**

**No bucket exists.** There is no mesh anywhere in the project with `bucket` or `pail` in its name -
zero hits. The nearest stand-ins are all in the Dreamscape farmlands pack: **`SM_Pot`**,
**`SM_Basket`**, **`SM_Jug`**. Which one reads as a fire bucket is a judgement by eye, and this
project has a standing rule that visual choices go to Michael with the options rather than being
iterated on by an agent. Once picked, the bucket wants a holder - the existing weapon-data-asset path
is how characters hold things today, so `DA_Weapon_Bucket` pointing at the chosen mesh is the
cheapest honest route, even though a bucket is not a weapon.

**The bucket brigade is a system, not a data tweak, and is deliberately not started.** It needs
`BT_Civilian` (which is why the Civilian row's `BehaviorTree` is unset rather than pointed at
something wrong), a fire-seeking behaviour that reads `UGSFlammableComponent`, the carry/douse verbs,
and the disbelief -> panic states GDD 8 describes. `NEXT` already carries
*"Civilians + livestock (routines, disbelief, brigade, flee)"* as blocked on `BT_Civilian`. This
ticket makes that work possible - there is now a civilian archetype for it to attach to - but does
not attempt it.

**Left alone deliberately:** `BP_UrielAPlotexia` is also archetype `Militia` with an arming sword.
Michael's ruling is that Uriel gets armour and more planning later, so he is **not** swept into this
change despite matching the pattern.


---

### The bucket, 2026-08-24

Michael: *"Use the Basket for the prop."*

`DA_Weapon_Bucket` created by duplicating `DA_Weapon_ArmingSword` (so the socket and offset fields
keep an authored shape) and overriding what a basket differs in:

- `MeleeMesh` = `SM_Basket`, on `hand_r_weapon`
- **scale 0.4** - `SM_Basket` measures **70.4 x 70.4 x 59.0 uu** at scale 1, which is a floor basket.
  A hand-carried bucket wants roughly 25-30uu across, so 0.4 gives 28uu wide by 24 tall. **That is a
  measurement and a starting point, not a judgement** - whether it reads as a bucket in his hand is
  Michael's eye, and the number is here so it can be nudged without re-deriving it.
- `BaseDamage` 0 and `AttackRange` 0 - a bucket is not a weapon
- `HornMesh` cleared - the sword asset carries `SM_HuntingHorn_Signal01` and a civilian has no
  business with the war horn
- `ShowHolsteredWeapon` false - no basket strapped to his back

`BP_PeasantMan.DefaultWeapon` -> `DA_Weapon_Bucket`.

**Observed:** in PIE on `L_Tutorial_Island` the peasant reports `EquippedWeapon = DA_Weapon_Bucket`
and carries `SM_Basket` attached at `hand_r_weapon` at scale 0.4.

**Not observed: what it looks like.** A viewport capture was attempted and framed the wrong part of
the level; rather than iterate on camera placement, the measurements above are handed over instead.
This project's standing rule is that mesh scale and framing are judged by eye by Michael, with the
agent supplying the numbers.

**Still not done, and still a separate piece of work:** the bucket brigade itself - `BT_Civilian`, the
fire-seeking behaviour against `UGSFlammableComponent`, and the disbelief -> panic states. The
civilian archetype and the prop now exist for it to attach to.
