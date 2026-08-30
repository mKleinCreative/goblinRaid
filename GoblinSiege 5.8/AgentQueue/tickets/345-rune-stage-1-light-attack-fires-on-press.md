---
id: 345
title: Rune stage 1: light attack fires on press, heavy moves to a short hold, dodge travels and grants i-frames, plate deflect reads as a deflect
agent: claude-combat
status: done
claimed: 2026-08-29T00:39Z
build: 2026-08-29 BUILD SUCCEEDED
waiting_on:
evaluated: 2026-08-29T02:49:46Z
observed: 2026-08-29T02:50Z | Michael confirmed light attack fires on mouse-down and the held-button heavy follows it - the mushy-input complaint is closed. Dodge measured in PIE: 359uu at the old 3x speed clamp, 417uu after raising it, against ~198uu before the change; MaxWalkSpeed observed going 470 -> 1410 -> 2820 and restoring to 470 every time, so no leak. i-frames verified at runtime: the live pawn holds State.Dodging and State.Invulnerable during the roll.
scenario: PIE on L_Tutorial_Island with the possessed BP_GSPlayerCharacter_C_0 (alive, health 100); dodge triggered from Python and start/end locations differenced across frames, plus Michael playing the melee directly.
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
  - Content/Blueprints/Abilities/GA_GS_Dodge.uasset
---

## Goal

Rune stage 1: light attack fires on press, heavy moves to a short hold, dodge travels and grants i-frames, plate deflect reads as a deflect

## Generate

Michael's four complaints, in his words: "no reaction to my hits", "one button, no decisions",
"mushy timing / input feel", "dodge doesn't seem to cover enough distance."

**1a. The light attack now fires on PRESS.** `GSPlayerCharacter.cpp` - `Input_AttackPressed` calls
`Input_Attack` immediately; `Input_AttackReleased` no longer attacks at all, it only clears the
charge timer and ring. `HeavyHoldSeconds` 1.5 -> **0.35**.

The heavy is *queued*, not fired on the spot: `TriggerHeavyAttack` sets `bHeavyQueuedThisHold` when
a swing is in flight, and a new `HandleAbilityEnded` (bound to the ASC's `OnAbilityEnded` in
BeginPlay) delivers it when the light chain ends. **The heavy is a different ability class from the
light, so activating it directly would not be refused** - GAS would run both and the player would
get two overlapping damage windows from one button. Queueing also means no visible swing is ever
cancelled, which is what makes a charge attack feel like a dropped input.

Added `FindActiveSwing()` and refactored `Input_Attack`'s inline spec loop to use it, so "which
instance owns the chain" is answered in one place.

**1b. The dodge.** Three separate causes, found in order:

1. *Friction.* `LaunchCharacter` sets a velocity that `GroundFriction` (8) and
   `BrakingDecelerationWalking` (2792) then ate within a couple of frames. Both are now zeroed for
   the roll and restored in a new `EndAbility` override - **restored there and not in
   `OnDodgeFinished`, because a roll cancelled by death must put them back too**; a character left
   at zero friction slides for the rest of the raid.
2. *The walking speed cap.* Measured live: `MaxWalkSpeed = 470`, and walking mode re-clamps velocity
   to it every frame, so `DodgeSpeed` 1500 / 2400 / 4800 all produced the same roll. A
   `UGSGE_MoveSpeedScalar` now lifts the ceiling for the roll's duration and is removed by handle in
   `EndAbility`, per the standing rule against writing `MaxWalkSpeed` directly.
3. *The attribute clamp* - see #348. `MoveSpeedMultiplier` was clamped to 3.

`DodgeSpeed` 900 -> 4800 (set on the Blueprint CDO as well as the C++ default, since the BP value
masks it).

**1c. The knight.** `GS.Combat.PlateFrontalScalar` 0.3 -> **0.6** in
`GSDamageExecCalculation.cpp`. A frontal player swing was `25 x 0.3 - 6 = 1.5` against 75 health -
fifty swings - where a flank skips the plate for full 25 in three. The counter was right and the
number made it academic. 0.6 gives ~9, so flanking stays clearly best and the front stays clearly
worse.

## Evaluate

**Observed by Michael:** light on mouse-down and the held heavy both work.

**Measured, not eyeballed** (the first honest numbers in this ticket - see the corrections below):

| Change | Before | After |
|---|---|---|
| Roll distance | ~198uu (calculated) | 359uu at 3x clamp, **417uu** at 6x |
| `MaxWalkSpeed` during roll | 470 | 1410 -> 2820, restoring to 470 every time |
| i-frames | none (`IFrameEffectClass` null) | `State.Invulnerable` present on the live pawn mid-roll |

**Three measurement errors I made, recorded because each was nearly reported as a finding:**

1. I sampled `Velocity` in the same frame as `LaunchCharacter`. It stores a *pending* launch applied
   on the next movement tick, so a synchronous read always shows 0. I nearly reported "the launch
   produces no velocity".
2. I picked the player by searching actors for "Player" in the name and got a **corpse** - health 0,
   `IsAlive False`, `movement_mode MOVE_NONE`. A dead pawn cannot move, so the roll measured zero.
   Michael caught it: "you were measuring a dead body, I was in a different character." Use
   `GetPlayerController(0)->GetControlledPawn()`, never a name match.
3. Earlier in the session I read a `GameplayTag`'s Python repr `{}` as "empty". It prints `{}`
   whether or not it holds a value; `get_tag_name()` is the only honest read.

**The cap was not the limiter, and this is unresolved.** Raising the ceiling from 1410 to 2820
uu/s moved the roll from 359uu to 417uu - a 16% gain where the arithmetic predicts ~100%. At
2820uu/s with friction zeroed, a ~0.9s roll should cover well over 2000uu. Velocity is therefore
being destroyed within a frame or two of the launch by something that is *not* `GroundFriction`,
`BrakingDecelerationWalking` or `MaxWalkSpeed` - all three were measured and excluded. Finding it
needs per-frame velocity sampling through the roll. **Filed as its own ticket rather than guessed
at again.**

**Not done, and deliberately:** the **deflect cue** from the original plan. A "your hit was refused"
cue only reads against a normal impact, and there is currently no impact layer at all - no hitstop,
no camera shake, no impact FX, no combat SFX. Building the cue before Stage 2 would be a cue against
silence. The plate *number* is fixed; the *feedback* belongs with Stage 2.

**AGENT_STATE.md owes:** DECISION - *the light attack resolves on press and the hold upgrades the
buffered follow-up; melee input no longer waits for button release.*

## Refine

**Changed in response to my own evaluation:** the heavy was originally going to activate directly at
the charge threshold. Writing it that way made the double-damage-window problem obvious (different
ability class => GAS runs both), so it became a queue delivered on `OnAbilityEnded`.

**Deliberately left undone:**

1. **Roll distance is 417uu, not the ~700 Michael asked for.** Banked deliberately: the dodge now
   travels, has working i-frames and restores its state cleanly, which were the three things
   actually broken. The remaining distance is polish and is blocked on the velocity question above.
2. **The i-frame window is 0.22s against a ~0.9s roll commit**, so the player is vulnerable for
   three-quarters of the roll. Michael adjusted the GE himself during the session; the mismatch is
   noted here because it is a tuning decision, not a defect - the effect provably applies.
3. **`bCommitForFullMontage` still stretches the commit to the montage length.** Worth revisiting
   with the roll distance, since commit length and travel are the same feel question.
