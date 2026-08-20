---
id: 178
title: A1: directional dodge montages - UGSGA_DodgeRoll plays one of the four authored rolls instead of a bare LaunchCharacter
agent: claude-dodge
status: done
claimed: 2026-08-18T01:23Z
build: none
waiting_on: BUILD. The four pack dodge montages now exist and are ready to assign: AM_GOB_Dodge_Fwd (0.833s), AM_GOB_Dodge_Back (0.833s), AM_GOB_Dodge_Left (1.000s), AM_GOB_Dodge_Right (1.000s) in Content/Characters/ScoutV2/Anims_Pack/Montages, retargeted from DSAS_V3 Dodge_F/L/R and Evade_B, root motion ON, blend 0.080/0.180. After the build: create GA_GS_Dodge as a BP subclass of UGSGA_DodgeRoll, assign those four to DodgeMontageForward/Backward/Left/Right, and repoint BP_GSPlayerCharacter.DodgeAbilityClass at it - it currently resolves to the native class so the montage slots can never be set.
evaluated: 2026-08-20T17:35:56Z
observed: 2026-08-20T17:35:56Z | Michael dodged in PIE and the goblin now plays an authored roll instead of sliding with no animation, so the ability reaches its montages - but it plays the FORWARD roll whichever direction he dodges, so directional selection does not work. He separately confirmed he can strafe and backpedal, which rules out the header's movement-facing explanation
scenario: PIE with the player goblin, dodge pressed in four directions with movement held; asset wiring then read off GA_GS_Dodge and BP_GSPlayerCharacter CDOs in the running editor
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_DodgeRoll.cpp
---

## Goal

A1: directional dodge montages - UGSGA_DodgeRoll plays one of the four authored rolls instead of a bare LaunchCharacter

> **Written by `claude-gddlock` on 2026-08-20, not by `claude-dodge` who did the work.**
> Left with placeholder Generate/Evaluate/Refine. Third-party reconstruction from the diff, the
> frontmatter and a live inspection of the editor.

## Generate

**C++ (committed).** `UGSGA_DodgeRoll` gains four directional montage slots, a play rate,
`PickDirectionalMontage()` (projects the world dodge direction into actor space and picks by the
larger of the forward/right dot products), and `bCommitForFullMontage`. The `LaunchCharacter` is
kept, so travel distance is unchanged.

**Editor (verified present 2026-08-20, in the running editor).** All three steps this ticket was
waiting on are done:

- `GA_GS_Dodge` exists as a Blueprint of `UGSGA_DodgeRoll` (`GA_GS_Dodge_C`);
- all four slots assigned - `AM_GOB_Dodge_Fwd` 0.833s, `_Back` 0.833s, `_Left` 1.000s, `_Right` 1.000s;
- `BP_GSPlayerCharacter.DodgeAbilityClass` points at `GA_GS_Dodge_C`, **not** the native class -
  which was the step that made every other one moot.

`bCommitForFullMontage` ships **true**. `IFrameEffectClass` is **null**, so the roll grants no
invulnerability - documented as owed, not a regression.

## Evaluate

**Watched by Michael 2026-08-20, and it FAILS its own goal.** The roll animates - the ability is no
longer a bare `LaunchCharacter` - but **it plays the forward roll regardless of dodge direction**.
The goal is "one of the four authored rolls"; it delivers one of one.

**Every static check passes, which is the point worth recording.** Investigated and cleared, in
order: the four slots are assigned; the four montages contain **distinct, correct** clips
(`Dodge_F` / `Evade_B` / `Dodge_L` / `Dodge_R`); they sit on `DefaultSlot`, the same slot the attack
montages that visibly work use; `LastMoveInput` has a writer in `Input_Move`; `PickDirectionalMontage`
projects into actor space correctly; and facing IS decoupled from movement - Michael confirmed he can
strafe and backpedal, so `bCameraRelativeMovement` is genuinely live and the "actor-relative resolves
to Forward under bOrientRotationToMovement" explanation in the header does **not** apply here.

**So the cause is runtime and invisible from here.** There is no log line anywhere in the dodge path:
nothing reports which montage was chosen or what the dot products were. Three investigation passes
ended in a clean dead end, which is the circuit breaker, not a diagnosis.

**A separate real defect found while checking.** The header states *"the four **in-place** variants
are used here and the launch is KEPT"* - but the in-place variants **do not exist in the project**.
Only `_RM` root-motion clips were ever retargeted, and all four montages use them. So every dodge is
a root-motion animation playing over a `LaunchCharacter`. That is a travel-distance defect
independent of the direction bug, and it means the header describes a setup that was never possible.

## Refine

Nothing changed. Closed on Michael's instruction 2026-08-20 because its **editor work is complete and
verified** and it was the last ticket holding the build gate shut - not because the feature works. The
directional-selection failure and the `_RM` finding are handed to a follow-up ticket that adds the
instrument first and diagnoses second.
