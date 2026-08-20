---
id: 187
title: Interact prompt: OnFocusChanged finally bound, and a locked container tells you to smash it
agent: claude-prompt
status: done
claimed: 2026-08-18T03:32Z
build: none
waiting_on:
evaluated: 2026-08-18T23:26:17Z
observed: 2026-08-18T23:25:38Z | Michael played it and confirmed it works: pressing F at an unsmashed container gives the reticle a short sideways shiver and no text, and once the container is broken open a gold prompt naming the verb appears under the reticle. His verdict: '187 works perfect.' The 7px / 0.22s / 14Hz shake and the 96px prompt drop both read correctly on the first set of values and needed no tuning.
scenario: PIE in L_CombatArena with BP_LootCrate_TEST and BP_LootBarrel_TEST (both shipping bIsAvailable=false) and BP_LootChest (available): press F while locked, smash, then look again
files: 
  - Source/GoblinSiege/Interaction/GSInteractableComponent.h
  - Source/GoblinSiege/Interaction/GSInteractableComponent.cpp
  - Source/GoblinSiege/Interaction/GSInteractionComponent.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/Blueprints/Interactables/BP_LootCrate.uasset
  - Content/Blueprints/Interactables/BP_LootBarrel.uasset
---

## Goal

Interact prompt: OnFocusChanged finally bound, and a locked container tells you to smash it

## Generate

The last of `UGSInteractionComponent`'s four delegates finally has a consumer, plus a fifth added for
refusals. #163 asked for a channel bar and an interact prompt; #169 built the bar, this is the prompt.

**Michael's rulings (2026-08-18):**

1. **The prompt does not name the key.** "Loot the barrel", not "Hold F to loot the barrel". The
   binding has already been remapped once (#058); baking the key into every prompt would make the next
   remap a content pass over every interactable.
2. **Locked containers get a reticle SHAKE, not a caption.** His words: *"is it possible to make the UI
   reticule jiggle slightly to give you the indication you can't interact with the item."* This
   replaced a `LockedPromptText` property that had already been written - see Refine.

**`CanFocus` split out of `CanInteract`** (`GSInteractableComponent`). `RefreshFocus` filtered on
`CanInteract`, which fails on `!bIsAvailable`, so a locked crate could never become focus. Now two
questions: focus asks *"what am I looking at"*, interaction asks *"may I do this"*. Widening focus is
safe by construction - every path that starts a channel (`ResolveChannelTarget`, `StartChannelInternal`,
`ServerBeginChannel`) tests `CanInteract` independently, so a locked container became visible, not
usable.

**An availability bonus in the focus score.** `+10` for available, against a `FacingDot` of at most 1
and a distance penalty of at most 0.25 - so an available interactable ALWAYS outranks a locked one.
Without it, the smashed barrel and the locked crate 120uu away in `L_CombatArena` compete on geometry
alone, and standing slightly nearer the crate would steal the prompt from the thing you can actually
loot.

**`OnInteractRefused`**, broadcast from `BeginChannel` when `ResolveChannelTarget` fails **and
something was focused**. A press into empty air is deliberately NOT broadcast: shaking for it would
train the player to ignore the shake.

**The shake** (`UGSPlayerHUDWidget::TickRefusalShake`): damped horizontal oscillation on the reticle's
render transform, 7px peak, 0.22s, ~14Hz, amplitude falling with time remaining so it settles rather
than stopping mid-swing, and landing exactly on zero - a sub-pixel residue would walk the reticle
off-centre after enough refusals, and the reticle is what the player aims with. Sideways only: a
vertical shake on a centre-screen reticle reads as the camera moving rather than the UI answering.
Re-triggering restarts rather than accumulates, so mashing F cannot produce a permanent vibration.

**HUD prompt:** `InteractPrompt` TextBlock in `WBP_GSPlayerHUD`, centred 96px below the reticle, gold,
`Collapsed` for no focus **and for locked focus**.

## Evaluate

**BUILT AND OBSERVED.** The build succeeded (39s, zero errors, DLL newer than every source file), and
Michael played it: pressing F at an unsmashed container shivers the reticle and shows no text; once
smashed open, the gold prompt appears. His verdict: *"187 works perfect."*

**The tuning values were right first time, which is worth flagging as luck rather than judgement.**
7px / 0.22s / 14Hz and the 96px prompt drop were all guesses - the Evaluate written before the build
called the shake "the risky half" and specifically doubted whether 7px (about 5% of the reticle's
128px width) would even be visible. It was. They stay `EditDefaultsOnly` regardless: the next person
to change reticle size will need them.

**Reflection data confirmed present after the build** - `FGSOnInteractRefused`, the `OnInteractRefused`
property, the HUD's `HandleInteractRefused` binding and `CanFocus` all appear in the generated code. My
first check reported `OnInteractRefused` missing, which was wrong: I grepped only `.generated.h`, and
delegate properties are emitted into `.gen.cpp`. **A verification that looks in the wrong file reports
a false failure just as confidently as a true one.**

**Still unproven, and both are honest gaps:**

- **Put-down behaviour with full hands near a locked container.** A locked crate now reaches
  `ResolveChannelTarget`'s fall-through where it was previously never focus at all. Reading the code
  that path is identical to "no focus", so a goblin with full hands still puts down - but the tested
  scenario was empty-handed, so this remains reasoned rather than watched.
- **Multiplayer.** `OnInteractRefused` is broadcast locally from `BeginChannel`, which is correct for a
  cosmetic response, but no client has ever run this.

## Refine

**Changed after self-review:** the first pass swapped `RefreshFocus` onto `CanFocus` and stopped, which
introduces a regression - the locked crate outscoring the lootable barrel 120uu away in
`L_CombatArena`, silently stealing the prompt from the thing the player can actually use, and reading
as this feature breaking looting. The `+10` availability bonus went in before anything was written up.

**A property was written and then deleted, and that is recorded rather than hidden.**
`LockedPromptText` ("Smash it open") was fully implemented before Michael proposed the shake instead. His
instinct was better: a caption under the reticle every time the player glances at an unsmashed crate is
exactly the nagging `ReticleIdleColour` was chosen to avoid, and it spends reading time on something a
nudge conveys instantly. **Deleted rather than left as a dormant option**, so the next person is not left
guessing which of two mechanisms is intended. Nothing was lost but the writing - the assets never took
the value, because the pre-build binary had no such property to set, and that silent failure is what
surfaced the timing problem in the first place.

**Deliberately not doing: a shake on pressing F at nothing.** Only a focused-but-refused press shakes;
shaking at empty air would train the player to ignore the signal.

**Deliberately not doing: sound.** A refusal thunk would carry this better than any visual, and the
project has no UI or smash SFX at all. That is a real gap and it is not this ticket's to fill.

## Refine

**Changed after self-review:** the first pass swapped `RefreshFocus` onto `CanFocus` and stopped, which
introduces the regression above - the locked crate outscoring the lootable barrel beside it, reading as
this feature breaking looting. The availability bonus went in before anything was written up.

**A property was written and then deleted, and that is worth recording rather than hiding.**
`LockedPromptText` ("Smash it open") was fully implemented before Michael proposed the shake. It was
the wrong answer and his instinct is better: a caption under the reticle every time the player glances
at an unsmashed crate is exactly the nagging `ReticleIdleColour` was chosen to avoid, and it costs
reading time to say something a nudge says instantly. **Deleted rather than left in as a dormant
option** - shipping both would leave the next person guessing which is the intended one. Nothing was
lost but the writing: the assets never took the value, because the pre-build binary had no such
property to set.

**Deliberately not doing: a shake on pressing F at nothing.** Only a focused-but-refused press shakes.

**Deliberately not doing: sound.** A refusal thunk would carry this better than any visual, and the
project still has no smash or UI SFX at all. That is a real gap, and it is not this ticket's to fill.
