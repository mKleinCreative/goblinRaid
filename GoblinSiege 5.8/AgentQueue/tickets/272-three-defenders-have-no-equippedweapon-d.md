---
id: 272
title: The no-EquippedWeapon warning fires on six defender Blueprints that are correctly armed and correctly statted - it is the #268 false positive one class over
agent: claude-warren
status: review
claimed: 2026-08-24T02:18Z
build: required
waiting_on:
evaluated:
observed: 2026-08-24T02:28:36Z | GS.Stats.Dump printed Erika at 20/20 health and both castle guards at 30/30, with ARS and the GS attribute set agreeing on all seven characters and zero mismatches - so the warning claim that they fight on 100/100/0 is false, and their health matches the GDD roster exactly. The same characters had already been seen arming themselves 4ms after the warning fired.
scenario: PIE on L_CombatArena with all six defenders spawned - two Erikas, two CastleGuard01, two CastleGuard02 - plus the player; console command run against the live game world.
files: 
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - docs/decisions-ledger.md
---

## Goal

Find out why `BP_CastleGuard01`, `BP_CastleGuard02` and `BP_ErikaArcher` log "has no EquippedWeapon
data asset ... keeps the attribute set's constructor defaults (100/100/0)" on every PIE start, and
propose a fix.

**The title this ticket was claimed under asserted the 100/100/0 as fact. It is not true** - it was
taken from the warning's own text. The measurement below disproves it. The title has been corrected
rather than left standing, because a false claim in a ticket title outlives the ticket.

## Generate

**Nothing was changed. This is an analysis ticket plus one runtime measurement.** Investigated with a
five-agent workflow - four independent read-only angles (the component's null path, the weapon
data-asset inventory, the design intent for defender stats, and whether attributes arrive by a
parallel path), then a synthesis.

**The finding: this is the #268 false positive again, in the class next door.**

The warning is emitted from the `else` branch of a null check on the *component's* own
`EquippedWeapon` - `Source/GoblinSiege/Weapons/GSWeaponComponent.cpp:35-68`, text at `:59-66`. That
branch only logs; nothing is torn down or half-initialised.

`AGSEnemyCharacter::BeginPlay` calls `Super::BeginPlay()` first - which dispatches component
`BeginPlay` and fires the warning - and only then arms the character from a **different property**
(`Source/GoblinSiege/Characters/GSEnemyCharacter.cpp:53-57`):

```cpp
if (WeaponComponent && DefaultWeapon)
{
    WeaponComponent->EquipWeapon(DefaultWeapon);
    WeaponComponent->SetSlot(DefaultSlot);
}
```

`DefaultWeapon` lives on the **character** (`GSEnemyCharacter.h:80`); `EquippedWeapon` lives on the
**component** (`GSWeaponComponent.h:307-309`). The diagnostic only knows about the second. A
correctly-armed defender therefore warns, then arms itself microseconds later in the same frame.

`AGSHordeGoblin` suppresses exactly this with one constructor line - `GSHordeGoblin.cpp:47`,
`WeaponComponent->bExpectsExternalEquip = true;` - and the component header documents the whole trap
and records that this warning has already caused two misdiagnoses (`GSWeaponComponent.h:370-386`).
`AGSEnemyCharacter`'s constructor creates the same component and never sets the flag
(`GSEnemyCharacter.cpp:9-36`). **That omission is the entire defect.**

The log shows the contradiction directly for Erika, 4ms apart, in three consecutive runs
(`MyProject.log:3452-3454`, `:3657`, `:4050`): the warning, then a `back_bow` socket rebuild, then
`BP_ErikaArcher_C_1 is now holding Bow` - both of the latter reachable only through `EquipWeapon`.

## Evaluate

**The measurement that settles it had never been run.** `GS.Stats.Dump` existed
(`Source/GoblinSiege/Combat/GSDebugCommands.cpp:1028-1112`) and appeared nowhere in any log. The
whole argument was about a number nobody had looked at. Run 2026-08-24 in PIE on `L_CombatArena`
with all six defenders present:

```
[GS.Stats] BP_ErikaArcher_C_1      ARS 20/20 hp  100/100 stam  def 0  |  GS 20/20 hp  armor 0
[GS.Stats] BP_ErikaArcher_C_2      ARS 20/20 hp  100/100 stam  def 0  |  GS 20/20 hp  armor 0
[GS.Stats] BP_CastleGuard01_C_2    ARS 30/30 hp  100/100 stam  def 0  |  GS 30/30 hp  armor 0
[GS.Stats] BP_CastleGuard01_C_1    ARS 30/30 hp  100/100 stam  def 0  |  GS 30/30 hp  armor 0
[GS.Stats] BP_CastleGuard02_C_1    ARS 30/30 hp  100/100 stam  def 0  |  GS 30/30 hp  armor 0
[GS.Stats] BP_CastleGuard02_C_0    ARS 30/30 hp  100/100 stam  def 0  |  GS 30/30 hp  armor 0
GS.Stats.Dump: 7 character(s), 0 MaxHealth mismatch(es).
```

**The warning's stated consequence is false.** Health is not 100/100. Erika is 20 and the guards are
30, which is exactly the GDD roster (`docs/goblin-siege-gdd.md:299-300`: "Militia 30 HP, Archer 20 HP
... Knight 75 HP / armor 6"). ARS and the GS attribute set agree on every row - zero mismatches - so
neither the archetype path nor ACF's initialiser skipped. **These characters are correctly statted
and correctly armed. There is no gameplay defect here.**

This also disposes of a live worry rather than assuming it away: the same log carries
`Invalid Ability Set` and `Invalid Character` ACF errors on these exact actors in the same
millisecond (`MyProject.log:3450-3451`), and #227 was precisely a silent ACF-init skip. The dump
shows ARS populated, so that failure did not occur here. Those ACF errors remain unexplained but are
not causing this.

**One of the four angles reached the opposite conclusion** - that Erika "is running on 100 HP", a
live defect five times the designed 20. Its stat research was sound; its premise was not. It took the
health figure from the warning's own text rather than measuring it, which is the exact trap the
component header documents. The measurement above overrules it. Recorded rather than quietly dropped,
because the disagreement is the useful part: three of four agents and the log all pointed one way,
and the fourth was wrong for a reason this project keeps repeating.

**The warning is also textually wrong about its mechanism, even for a genuinely unarmed character.**
Health has not come from `InitialAttributesEffect` since #228 - `AGSCharacterBase::GetHealth()` reads
ACF's `UACFStatisticsSet` first (`GSCharacterBase.cpp:373-403`) - and all five weapon data assets
have `InitialAttributesEffect = None`, so the clause names a field nothing populates.

**Not established:** the field values on the five weapon data assets were read from the editor by an
investigator, not re-verified by me. Nothing in the conclusion rests on them.

## Refine

**Recommended fix - one line and one comment. Needs a build, so it has not been made.**

1. `Source/GoblinSiege/Characters/GSEnemyCharacter.cpp`, in the constructor beside the
   `WeaponComponent` creation (`:35`):

   ```cpp
   WeaponComponent->bExpectsExternalEquip = true;
   ```

   Constructor, not `BeginPlay` - component `BeginPlay` runs first, which is why the goblin's
   equivalent line lives there. This clears the warning for **six** Blueprints, not three:
   `BP_KnightDPelegrini`, `BP_PeasantMan` and `BP_UrielAPlotexia` are in the identical state and will
   warn the moment they are spawned.

2. Fix the warning text at `GSWeaponComponent.cpp:59-66` in the same edit. Two of its three clauses
   are false regardless of equip state. Proposed: name the field and the remedy, drop the attribute
   claim entirely - *"has no EquippedWeapon on its WeaponComponent and does not expect an external
   equip - no weapon meshes and no weapon-granted abilities. Set EquippedWeapon on the Blueprint's
   WeaponComponent, or set bExpectsExternalEquip if the owner equips in BeginPlay."*
   **A diagnostic that has now produced three misdiagnoses is worse than none.**

**Alternatives rejected:**

- *Assign `EquippedWeapon` on the six Blueprints' component templates.* This silences the warning by
  duplicating the weapon into a field that does not win - `DefaultWeapon` is what `BeginPlay`
  actually uses - and edits six `.uasset`s to paper over a one-line C++ omission. It is what #144 and
  #267 proposed, and both were wrong.
- *Move the check to a next-tick or PostBeginPlay timer.* `bExpectsExternalEquip` already exists for
  this and is documented as constructor-set for this reason (`GSWeaponComponent.h:378-384`).
- *Raise defender health or armour to match the GDD.* Rejected: the measurement shows health already
  matches. There is nothing to raise.

**A separate question for Michael - design, not a bug.** Both castle guards are archetype `Militia`,
so **armour 0**. `GSDamageExecCalculation.cpp:328` keeps `FinalDamage = DamageAfterRace` whenever
`Armor <= 0`, which means the entire directional-plate system from #091 - the 150-degree frontal arc,
flank and back bypass, `Damage.Bow` armour-ignore - **does nothing against them at any angle**. The
GDD names castle reinforcements (`goblin-siege-gdd.md:298`) without a stat line, and #087's log shows
`BP_CastleGuard01_C_0` taking a hit at `armor 6.0` historically, i.e. knight-class.

1. Which roster row are `BP_CastleGuard01/02` - Militia (30 / armour 0) or Knight (75 / armour 6)?
2. Should Highpurse castle reinforcements be a tougher class than the village's posted guards? §8's
   "the hamlet can't stop you; the county can" implies yes but attaches no numbers.

That belongs in its own ticket. #272 closes on the warning alone.
