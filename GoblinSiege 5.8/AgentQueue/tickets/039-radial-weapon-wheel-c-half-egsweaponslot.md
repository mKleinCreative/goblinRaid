---
id: 039
title: Radial weapon wheel, C++ half: EGSWeaponSlot replaces the bRangedMode bool, selection maths, BP-facing state and events
agent: claude-wheel
status: done
claimed: 2026-08-06T18:26Z
build: required
waiting_on:
evaluated: 2026-08-06T18:31:53Z
files: 
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

Radial weapon wheel, C++ half: EGSWeaponSlot replaces the bRangedMode bool, selection maths, BP-facing state and events

## Generate

The C++ half of the radial weapon wheel, per Michael's call today: code owns the selection maths and
exposes state + events; the UMG widget comes later and binds to them.

**`EGSWeaponSlot { Torch, Bow, Sword }` replaces the `bRangedMode` bool.** `HANDOFF.md` flagged this
as a type change rather than a bigger toggle, and it is - the bool could not name the torch at all,
because the torch was a one-shot ability that borrowed the hand for 0.25s. Enum order is the wheel's
own, clockwise from the top, so a widget iterating the enum gets the arrangement the maths produces.

**`SetSlot(EGSWeaponSlot)` is the single choke point.** Every path - wheel, legacy toggle, Blueprint -
goes through it, so mesh placement, the torch prop and the broadcasts cannot disagree about what is
held. It carries forward the three refusal reasons `ToggleRangedMode` split out on 2026-08-05 (swap
lock / no weapon / no ranged half), each naming its own cause, and adds the bow-specific one.
`ToggleRangedMode()` survives as a two-way sword<->bow flip - **the torch is deliberately not in that
cycle**, so a player who has never opened the wheel cannot end up holding one by tapping swap twice.

**Wheel API:** `OpenWeaponWheel()`, `AddWheelInput(FVector2D Delta)`, `CloseWeaponWheel(bool bCommit)`,
plus `IsWheelOpen`, `GetWheelHighlight`, `GetWheelVector`, `IsWheelCommitted`, and
`SlotForDirection(Direction, Fallback)` exposed BlueprintPure so the widget draws the sectors the
input actually uses instead of a second copy that can drift.

Accumulated mouse **delta**, per the settled design - the game has no visible cursor, and a
position-based wheel would need one warped to centre on open. `SlotForDirection` negates screen +Y to
get a conventional angle (up = +90), then takes 120-degree sectors centred on 90 / 210 / 330.
Inside `WheelDeadZone` it returns the fallback, which `OpenWeaponWheel` seeds with the CURRENT slot -
so "drag back to centre cancels" needs no special case, and a Q tap with no drag is a no-op rather
than arming you with whatever enum value happens to be 0.

**Events:** `OnWeaponSlotChanged`, `OnWheelOpenChanged`, `OnWheelHighlightChanged` (on change only,
not per frame). `OnWeaponModeChanged(bool)` is **kept and still broadcast**, so existing C++ and
Blueprint bindings survive - `AGSPlayerCharacter::HandleWeaponModeChanged` needed no edit at all.

**The torch stays in hand.** `SetTorchReadied(false)` is now refused while the Torch slot is
selected. `UGSGA_TorchToss` readies on activation and un-readies the instant the projectile spawns -
which IS Michael's "it's never in your hand", with the socket and mesh fine all along. The slot now
outranks the ability, so the torch comes back for the next throw because the goblin is still holding
it. Readying from another slot still works, so nothing that has not moved to slots changes.

`GetActiveWeaponMesh()` returns the MELEE mesh in the Torch slot - not null, which callers already
read as "no art yet", and not the torch, which the header is explicit must never be traced along.

## Evaluate

**NOT COMPILED. The editor is deliberately OPEN** for Michael to PIE-verify #037's adopt-radius
change, which is the riskier item, so this ticket's build is deferred to the next time it closes.
That is a choice, not an oversight - but it means this is the largest unverified change of the
session, and it is a type change across a dozen call sites.

**Nothing here is wired to any input.** `OpenWeaponWheel` / `AddWheelInput` / `CloseWeaponWheel` have
no caller: Q is not bound, mouse delta is not fed in, and the attack button still routes on
`IsRangedAttackMode()` exactly as before. **So today this changes nothing a player can do** - the
sword/bow swap behaves identically and the torch behaves identically, because nothing ever selects
the Torch slot. That is the correct stopping point for "C++ layer now, UMG after", but the feature
is not testable end to end until something calls it.

**Deliberately NOT done, and this is where I stopped short of the design:** the second settled
decision - "the ATTACK button throws what you're holding, `IA_ThrowTorch` retires" - is untouched.
It needs input-asset changes in the editor (retiring an IA, binding Q), so doing the C++ half alone
would leave two ways to throw a torch with different behaviour. Better as one piece with the widget.

**Verified only by reading and grep.** `bRangedMode` now survives only as the parameter NAME on the
compatibility delegate and in `HandleWeaponModeChanged` - confirmed by grep across `Source/`. 25
`EGSWeaponSlot` references. `AGSPlayerCharacter` required no changes, which is the main evidence the
compatibility shim works.

**Risks I can name:**
- **`WheelDeadZone = 40` is a guess.** Raw mouse-delta units are neither pixels nor degrees. It is
  `EditDefaultsOnly` so it is tunable without a rebuild, but it has never been felt.
- **The +Y-down negation in `SlotForDirection` is exactly the kind of thing that silently mirrors the
  wheel** top-to-bottom and reads as "the sectors feel wrong" rather than as an error. Untested.
- **`SetSlot` refuses under the anti-cancel lock**, which the wheel inherits from the swap key. A
  wheel selection landing inside 0.15s of a previous swap will be silently dropped (it logs). Whether
  that is right for a wheel - as opposed to a spammable toggle - is a feel question nobody has asked.
- **`SetTorchReadied`'s new refusal is a behaviour change for existing callers.** `UGSGA_TorchToss`
  un-readying is now conditionally ignored. Correct for the slot design; it does mean the ability no
  longer fully controls the prop it thinks it owns.

**Touched outside the goal:** none. `AGENT_STATE.md` got the two settled decisions (arrow sticks in
allies; wheel is C++-first) written into DECISIONS so they cannot be re-litigated, which is what
`HANDOFF.md` Part 3 asks for. `GSPlayerCharacter.h/.cpp` were claimed but not modified.

**Owed AGENT_STATE.md** - the DECISIONS entries are written. No FAILED line: nothing failed here yet,
because nothing has run.

## Refine

- **Kept `OnWeaponModeChanged(bool)` instead of replacing it with the slot delegate.** Changing a
  `BlueprintAssignable` signature silently breaks any Blueprint bound to it, and `BP_GSPlayerCharacter`
  is exactly the kind of asset that binds one - a break I could not see from C++ and would not find
  until PIE. Additive costs one extra broadcast per swap.
- **Seeded the wheel highlight from the current slot rather than from the enum's first value.**
  First version defaulted to `Torch` (enum 0), which made a Q tap with no drag hand the player a
  torch. Seeding from `CurrentSlot` is also what makes "drag back to centre cancels" fall out for
  free instead of needing its own branch.
- **Made `SetTorchReadied` refuse rather than making `UGSGA_TorchToss` stop calling it.** Editing the
  ability would have fixed the visible symptom for the torch slot and left every other caller free to
  yank the prop out of a held hand. The rule belongs where the state lives.
- **`ToggleRangedMode` stays two-way.** Folding the torch into the swap key would have been fewer
  lines and would have shipped a torch to players who never opened the wheel.
- **Left `GetActiveWeaponMesh` returning the melee mesh in the Torch slot, and said so at the call
  site.** Returning null was tempting and would have been read as "no art yet" by every existing
  caller - a different meaning wearing the same value.

**Deliberately left undone:** input binding (Q, mouse delta) and attack routing by slot - both need
editor-side input assets, and half-wiring them gives two ways to throw a torch; the UMG widget; the
build, which waits for the editor to close; and tuning `WheelDeadZone`, which cannot be done
honestly until there is something on screen to feel it against.
