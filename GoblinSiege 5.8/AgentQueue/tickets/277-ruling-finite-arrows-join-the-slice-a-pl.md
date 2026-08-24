---
id: 277
title: "RULING: finite arrows join the slice - a player-only quiver, against the 12.4 scope freeze"
agent: claude-warren
status: done
claimed: 2026-08-24T03:52Z
build: none
waiting_on:
evaluated: 2026-08-24T04:04:30Z
observed: UNOBSERVED 2026-08-24T04:04:31Z - A ruling and two document edits. Nothing runnable was produced, so there is nothing to watch. The claim it rests on WAS checked at runtime before the ruling was written - arrows are genuinely unlimited today, with no cost or cooldown effect on either ability CDO or on the GA_GS_TorchToss Blueprint. Stages 1-4 each carry their own observation.
scenario: none - never run
files: 
  - docs/decisions-ledger.md
  - docs/goblin-siege-gdd.md
---

## Goal

Stage 0 of the finite-arrows plan. **No code.** GDD §12.4 says *"nothing joins the tutorial without a
new ruling and a ledger row"*, and items / inventory / consumable resources appear in **no column** -
not IN, not CUT, not DEFERRED. Under §12.4 silence means "needs a ruling", so this is the gate the
rest of the work waits behind. Precedent: #252 did exactly this for world corruption
(*"it lands against §12.4's scope freeze - hence a ruling before a line of code"*).

## Generate

**`docs/decisions-ledger.md`** - new dated section, **rulings 46-52**. The lead ruling is that arrows become finite for the
**player only**. A starting quiver, spent per shot, refilled by walking over a bundle or a dead
archer. Torches stay infinite, AI archers never run dry, weapons are not lootable and every character
keeps its authored kit.

**`docs/goblin-siege-gdd.md`** - `| Finite arrows — a player-only quiver (46) | | |` appended to the
§12.4 IN column, and a `v1.2 | 2026-08-24 | #277` revision row citing rulings 46-52, which the change rule requires
(*"the ticket id goes in the revision row"*).

### The three objections, recorded because they are why the scope shrank

Michael asked for full scarcity on **both** arrows and torches. He was shown the following before
answering, and narrowed it himself. This is in the ledger so nobody re-discovers it and re-opens the
question:

1. **Torch-spam already has a settled answer, and it is scoring.** §10 prices a house at 10
   *precisely* so the tally is not *"decided by whoever had spare torches"* (ruling 21). Finite
   torches would re-solve a solved problem. **Torches were dropped from the scope entirely.**
2. **Bow-spam already has a settled answer too** - fire rate, #034. So finite arrows have to earn
   their place as their own feature, not as a second fix for something already fixed.
3. **The wood economy - the project's only resource-economy precedent - is DEFERRED to tier-2.** An
   arrow supply is structurally the same kind of system. Michael took it anyway; that is his call,
   and the ledger records the inconsistency as deliberate rather than unnoticed.

### The interaction that matters, and it is NOT a new ruling

The 2026-08-06 ranged ruling stands untouched: *an arrow STICKS in an ally, deals nothing, and the
shot is wasted* - taken with the horde case explicitly on the table, and marked "do not re-litigate".

**Finite arrows sharpen it.** With unlimited arrows that ruling cost a miss; with a quiver it costs a
consumable, so a horde goblin wandering into your line becomes a material loss. This ticket does not
re-open it and the plan does not touch it - but it is now harsher than when it was decided, and it
is flagged to be **watched in play** rather than discovered.

## Evaluate

`check_gdd.py`: **CLEAN, 10 checks passed** - run after the edits, which matters because the GDD has
a documented parser contract where a malformed table row is *silently dropped* rather than reported.
The §12.4 table is three columns with a trailing pipe and the new row matches the existing eight.

**Established, not assumed** - the load-bearing claim of the whole plan was checked at runtime before
this ruling was written: arrows are genuinely unlimited today. `CostGameplayEffectClass` and
`CooldownGameplayEffectClass` read `None` on `UGSGA_BowShot`'s C++ CDO, on `UGSGA_TorchToss`'s CDO,
**and** on the `GA_GS_TorchToss` Blueprint (checked separately, because a designer-authored cost
would live on the Blueprint and not the C++ class). There is no `GA_GS_BowShot` Blueprint at all. So
this is net-new behaviour with no existing mechanism to co-opt - not a port.

Also established: the counting backend already exists and is empty. `AGSCharacterBase` derives
`AACFCharacter`, whose constructor creates a `UACFEquipmentComponent` extending
`UACFInventoryComponent`. Every character already carries a replicated, stacking, weight-aware
inventory that nothing reads.

**Not established, and it cannot be from a ruling ticket:** whether finite arrows are *fun*. The
ledger records the design argument; only play settles the rest, and the arrow-in-an-ally interaction
above is the specific thing to judge it on.

## Refine

**Scope shrank in response to the evaluation, before anything was written.** The first version of
this ruling covered arrows *and* torches, because that is what was asked for. Reading §10 and
ruling 21 first turned up that torch supply is already governed by score weighting, so putting the
objection to Michael was worth more than implementing what was asked. He took arrows only. That is a
better ruling than the one this ticket was claimed to write.

**Deliberately left open, not decided here:** whether the weapon wheel migrates onto ACF equipment at
all (#274's open question). Arrows use ACF purely as a count store. `UGSWeaponComponent` keeps the
meshes, sockets, offsets and the wheel; `AGSArrowProjectile` keeps the shooting. ACF's
`UACFShootingComponent` was examined and **rejected** in the plan - adopting its ammo loop replaces
our arrow projectile and deletes the tuned mesh offset, the distance-to-head-bone headshot and the
bow timing minigame, to buy features this scope does not want.

**Nothing was built.** Stages 1-4 (the item, walk-over resupply, the gate and decrement, the HUD
count) are separate tickets and each needs its own observation. The full plan lives at
`C:\Users\Michael\.claude\plans\zesty-enchanting-quill.md`.


---

### Split into seven rulings rather than one, 2026-08-24

Written first as a single ruling 46 with the sub-decisions folded into prose, then split. The ledger
is this project's citation mechanism - rulings get referenced by number across tickets and code
comments ("ruling 21", "ruling 44", "ruling 43") - and prose inside one row cannot be cited. The
precedent is world corruption, which took six rulings (40-45) for one feature.

The rows that most needed to be individually citable:

- **47, torches stay infinite.** Without a row, a later agent reads "arrows are finite" and treats
  torches as merely not-done-yet rather than deliberately excluded.
- **49, spent arrows are litter.** This promotes `GSArrowProjectile.cpp:365`'s comment to a ruling.
  It was previously an implementation choice with a rationale, which anyone could reverse; it now
  needs a ledger row to undo. It is also load-bearing for 51.
- **51, `UACFShootingComponent` is rejected.** Deliberately in the same form as ruling 44 ("ACF
  contributes nothing here"), because the next person to look at ACF's ranged system will otherwise
  redo the whole audit.

Also recorded as **findings, not rulings**: the equipment component already exists on every
character, and `AACFPickup` does not work standalone. Both cost real investigation this session and
both would be re-investigated from scratch otherwise - the precedent for recording that kind of thing
is ruling 43.
