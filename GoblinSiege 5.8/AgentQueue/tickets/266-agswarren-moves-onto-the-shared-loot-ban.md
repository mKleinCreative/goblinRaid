---
id: 266
title: AGSWarren moves onto the shared loot bank component, so there is one banking implementation not two
agent: claude-warren
status: done
claimed: 2026-08-23T22:55Z
build: required
waiting_on:
evaluated: 2026-08-23T22:57:44Z
observed: 2026-08-23T23:15:06Z | A carried pig banked at the Warren for 40 loot AFTER the migration, logged by the shared component as: LogGSLootBank - The warren on BP_GS_Warren_C_0 swallowed BP_Livestock_Pig_C_0 for 40 loot. The LogGSLootBank category and the warren label prove the Warren is now running the shared implementation rather than its own deleted copy, and that the behaviour Michael had already signed off survived the refactor. The portal banked identically in the same session.
scenario: PIE in L_CombatArena, 2026-08-23 23:12 UTC, editor build of 16:09; player pawn carrying BP_Livestock_Pig_TEST into a T-planted Warren at (-2992,1171,-70).
files: 
  - Source/GoblinSiege/Raid/GSWarren.h
  - Source/GoblinSiege/Raid/GSWarren.cpp
---

## Goal

AGSWarren moves onto the shared loot bank component, so there is one banking implementation not two

## Generate

`AGSWarren` now delegates banking to `UGSLootBankComponent` instead of carrying its own copy. This is
the second half of the job #255 started: GDD 9 asked for one implementation with two consumers, and
for a few days there were two.

- **`CommitBank`, `GetLootValueOf` and the two running tallies are deleted from `AGSWarren`.** The
  component owns them.
- **`BankCarriedLoot` and `BankLooseActor` become thin forwarders**, and both keep the `bBanksLoot`
  check LOCALLY. That switch is a property of a particular Warren ("a decorative rune that takes
  nothing"), not of banking as an idea - the runic site has no equivalent and must not inherit one.
- **`OnLootBanked` is re-broadcast**, not replaced. `HandleLootBanked` forwards the component's
  delegate onto the actor's own, so any Blueprint already bound to `AGSWarren::OnLootBanked` keeps
  firing. The migration has to be invisible from outside the class.
- `GetPointsBankedHere` and `DescribeStatus` read through to the component, so `GS.Warren.Status` is
  unchanged.
- A doc comment pointing at `AGSWarren::CommitBank` was repointed at the component rather than left
  dangling at a function that no longer exists.

**Public surface deliberately unchanged.** Nothing outside this class needed editing.

## Evaluate

**NOT COMPILED, NOT RUN.** Gate closed; the editor was relaunched by the last build.

**Verified by evidence:** declarations cross-checked against definitions - five declared members,
five defined, no orphans either way; and a leftover scan confirming `CommitBank`, `GetLootValueOf`
and the tally increments have no live references left in `GSWarren.cpp`. **That is a spell-check,
not a compile.**

**The risk this change carries is specific and worth naming:** it re-plumbs the one part of the
Warren that has actually been WATCHED working - Michael banked a carried pig for 40 loot and dropped
loot into the mouth on 2026-08-21. Refactoring verified behaviour is exactly how a working feature
quietly stops working, which is why #255 deliberately did NOT do this in the same pass. **It must be
re-watched, not assumed.**

Two things most likely to break, in order:
1. **The delegate forward.** If `HandleLootBanked` is not bound, banking still works and scores
   correctly but any Blueprint hook on the Warren goes silent - a failure that looks like nothing at
   all.
2. **`bBanksLoot`.** It is now checked in the forwarder rather than deep in the logic. A Warren with
   it switched off must still take nothing.

## Refine

Nothing changed on review beyond the dangling doc comment.

**Deliberately left undone:** the `BeginPlay` sweep still lives on `AGSWarren` rather than moving into
the component. The runic site deliberately does NOT have one - the gate sits where the player spawns
and a sweep there would eat the starting dressing - so the sweep is Warren-specific behaviour and
belongs where it is.

> 2026-08-23T22:57Z Migration written, uncompiled. Re-plumbs watched behaviour - needs re-watching, not assuming.
