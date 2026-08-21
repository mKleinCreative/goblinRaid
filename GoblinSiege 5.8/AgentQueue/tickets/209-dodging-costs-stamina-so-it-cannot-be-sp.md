---
id: 209
title: Dodging costs stamina so it cannot be spammed - UGSGA_DodgeRoll gains a TryConsume gate
agent: claude-dodgestam
status: done
claimed: 2026-08-20T20:34Z
build: required
waiting_on:
evaluated: 2026-08-20T20:36:21Z
observed: UNOBSERVED 2026-08-20T20:36:22Z - Written, never run: the stamina gate needs a build, and this ticket's own claim on GSGA_DodgeRoll.cpp is what shuts the build gate. Same shape as 204, 205, 176, 179 and 189. No behaviour claimed - the cost has never been spent. The build runs next and Michael tests it.
scenario: none - never run
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
---

## Goal

Dodging costs stamina so it cannot be spammed - UGSGA_DodgeRoll gains a TryConsume gate

## Generate

`UGSGA_DodgeRoll` gains `DodgeStaminaCost` (EditDefaultsOnly, default **30**) and spends it through
`UGSStaminaComponent::TryConsume` at the very top of `ActivateAbility`.

Three placement decisions, each deliberate:

- **Before `CommitAbility`, not after.** A refused dodge must cost nothing; committing an ability we
  are about to cancel would burn any GAS cost or cooldown attached to it.
- **Before the launch, the montage and `State.Dodging`.** A dodge that cannot be paid for never
  moves the character and never locks movement input.
- **`FindComponentByClass`, not a reach through `AGSPlayerCharacter`.** The stamina component is
  deliberately player-only (the header says AI melee is paced by its own BT cooldown), so a null
  component is a legitimate "this pawn has no stamina model" and **must not refuse the dodge** -
  otherwise this would silently disable dodging for every AI that ever gets the ability.

A refusal logs under the existing `GS.Combat.LogDodge` cvar with the actual numbers, so "why did my
dodge not come out" is answerable rather than mysterious.

## Evaluate

**The number is sized against regen, and the arithmetic is the part worth checking.** The pool is
100, regen is 25/s with `RegenDelaySeconds` at 0, and a dodge commits for its montage length
(0.833s fwd/back, 1.000s left/right) because `bCommitForFullMontage` is true. So **21-25 stamina
comes back during the roll itself**. Any cost at or below that is free in practice and the spam this
ticket exists to stop would still work. At 30 the net is about **-8 per dodge cycle**: one or two
dodges in a fight are unaffected, sustained spam drains the bar over roughly a dozen consecutive
rolls.

**That is a throttle, not a hard stop, and I want that on the record.** If Michael wants dodging to
run dry fast, the better lever is `UGSStaminaComponent::RegenDelaySeconds` (currently 0) - a short
pause after ANY spend bites far harder than a bigger number here - but it also affects sprint and
climb, so it is a balance decision for him rather than something to change inside a dodge ticket.

For scale, the Blueprint's traversal moves cost 8 (vault) and 18 (mantle).

**NOT verified: nothing has run.** This is written and needs a build; the build gate is shut by this
ticket's own claim on the file, the same shape as #204, #205, #176, #179 and #189. No behaviour is
claimed.

**Deliberately NOT done:** suppressing regen for the duration of the roll. `SetRegenSuppressed` is
last-writer-wins and climb already uses it (#072/#076), so a dodge toggling it could clear a climb's
suppression and hand the player free stamina on a wall. That interaction is worth more thought than
a spam fix deserves.

## Refine

Sized the cost against regen after first reaching for a number between vault's 8 and mantle's 18 -
which the arithmetic then showed would be **entirely free**, because more than that regenerates
during the roll. Writing the interaction into the property's own comment rather than only the ticket,
because the next person to tune this will be looking at the property, not at me.
