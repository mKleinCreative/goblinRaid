---
id: 041
title: Editor half of the weapon wheel: IA_WeaponWheel + Q mapping, retire IA_ThrowTorch, WBP_WeaponWheel widget
agent: claude-wheel
status: done
claimed: 2026-08-06T18:50Z
build: none
waiting_on:
evaluated: 2026-08-06T18:58:22Z
files: 
  - Content/Input/IA_WeaponWheel.uasset
  - Content/Input/IMC_Default.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - Content/UI/WBP_WeaponWheel.uasset
  - decision-queue-kanban.html
---

## Goal

Editor half of the weapon wheel: IA_WeaponWheel + Q mapping, retire IA_ThrowTorch, WBP_WeaponWheel widget

## Generate

Editor-side wiring for the wheel, over MCP.

**1. `IA_WeaponWheel`** created in `/Game/Input` (BOOLEAN, consume_input), matching `IA_ThrowTorch`
which it replaces.

**2. Q remapped, which retires the old torch key in the same edit.** Q was already
`IA_ThrowTorch` -> so repointing that one row both binds the wheel and removes the second, unslotted
way to throw that #040 flagged as a live inconsistency. `IA_ThrowTorch` now has **zero** mappings.
The asset is kept (`ThrowTorchAction` still references it) so nothing dangles.

**3. `WeaponWheelAction` assigned** on `BP_GSPlayerCharacter`, compiled and saved.

**4. `GA_GS_TorchToss` Blueprint subclass created** in `/Game/Blueprints/Abilities`, and
`TorchTossAbilityClass` repointed at it. `TorchTossAbilityClass` was the raw C++ class, so
`TorchWindupSeconds` and `ThrowMontagePlayRate` were EditDefaultsOnly properties with no CDO to
edit - rebuild-to-change knobs, the same trap as `ArrowMeshOffset` in #034. Every other ability
(`GA_GS_Block`, `GA_GS_SwordLight`, `GA_GS_SwordHeavy`, `GA_GS_GuardBreak`) already exists as a BP
subclass, so this follows the project's own pattern rather than inventing one.

`TorchWindupSeconds` 0.25 -> **0.55**. `AM_GS_ThrowTorch` is 2.2s authored, played at 1.5, so 1.47s
on screen; 0.25s released the torch ~17% in, before a throw has wound up. **0.55 is an ESTIMATE**
(~37%) - the montage's real release frame is not readable from Python, its `notifies` array is
protected. It is now a Blueprint value, so tuning it costs nothing.

**Not done: `WBP_WeaponWheel`.** See Evaluate.

## Evaluate

**VERIFIED IN A LIVE PIE WORLD.** `L_Tutorial_Island`, `BP_GSPlayerCharacter_C_0`, driving the
component directly:

- Sword -> **Torch** (`torch_readied=True`), Torch -> **Bow** (`is_in_ranged_mode=True`,
  `torch_readied=False`), Bow -> **Sword** (`GetActiveWeaponMesh()` -> `GS_Sword`).
- Log agrees: exactly three `is now holding Sword/Bow/Torch` lines.
- No missing-mesh or missing-montage warnings, so every soft reference resolved.

**The selection maths was tested exhaustively off the CDO, no PIE needed**: up->Torch,
down-left->Sword, down-right->Bow, up-left and up-right->Torch, dead centre and a 10-unit twitch
both fall back. **The screen-space +Y negation is correct, not mirrored** - that was the risk #039
called out as unverifiable by reading, and it is now settled by evidence.

**A real bug surfaced, and it was my own predicted one.** Five gestures fired in a single frame:
the first committed and the next two were silently dropped. Cause is the 0.15s `bSwapLocked`
anti-cancel lock, which `SetSlot` inherits from the swap key - exactly the risk #039's Evaluate
named. The log carries 2 `anti-cancel lock is still up` lines, matching the 2 blocked gestures
precisely.

**I am NOT treating that as a defect, and the reasoning matters:** the test was artificial. No human
completes a hold-drag-release gesture in under 150ms, so in play the lock cannot bite the wheel. It
still means a wheel selection made immediately after a Tab swap is dropped with only a Log line.
Whether a wheel should honour a lock designed to stop frame-perfect combat cancels is a **feel
question that has not been asked**, and I have not answered it unilaterally.

**Not verified:**
- **Q itself.** I drove the component API, not the input path. `Input_WheelOpen` / `Input_Look`
  diversion / `Input_WheelClose` have never run - a keypress is the one thing MCP cannot fake here.
- **Nothing was thrown.** The torch montage, the new 0.55s windup, and ATTACK-routes-by-slot are all
  unexercised. 0.55 remains an estimate against an animation I could not read the release frame of.
- The **widget does not exist**, so there is no on-screen feedback: the wheel is invisible, and a
  player gets no indication of which sector they are in until they release.

**Touched outside the goal:** `GA_GS_TorchToss` and `TorchTossAbilityClass` were not in the original
claim - I claimed `BP_GSPlayerCharacter.uasset` but not the new ability BP. Small, and in service of
the same goal, but it is a claim I widened after the fact rather than before.

**Owed AGENT_STATE.md** - FAILED: *iterating a Python-exposed `Array` of structs yields COPIES.*
Mutating the loop variable and saving reports success and changes nothing; the first IMC edit
"succeeded" and the read-back still showed the old action. Assign back by index. This is the
"a successful tool call is not evidence" rule with a specific, repeatable cause.

## Refine

- **Re-read the IMC off a fresh load instead of trusting the save.** The first remap returned
  `SAVED: True` and had changed nothing, because the loop mutated struct copies. Only a
  `collect_garbage()` + reload caught it. This is the third time this session a call reported success
  and did nothing (the "Target is up to date" build, the stale-Evaluate gate, this) - re-reading is
  not caution, it is the minimum.
- **Repointed the existing Q row rather than adding a new mapping.** Adding one would have left Q
  bound to two actions and made the retirement of `IA_ThrowTorch` a separate, forgettable step.
- **Created a Blueprint subclass instead of rebuilding to change a number.** The estimate for
  `TorchWindupSeconds` is probably not exactly right - so the thing worth building was the ability to
  change it in two seconds, not the perfect first guess.
- **Split the same-frame gesture test into separate calls once it failed.** The obvious read of the
  first result was "SetSlot is broken". Spacing the calls across real frames proved the maths and the
  slot change were fine and isolated the lock - a bug report I would otherwise have filed against my
  own working code.
- **Left the anti-cancel lock alone.** Removing it for the wheel is a one-line change I could have
  made and justified; it is a feel decision on a rule that exists to stop combat cancels, and it is
  Michael's.

**Deliberately left undone:** `WBP_WeaponWheel`. It is a substantial UMG build and the wheel is
functional without it - `SlotForDirection` is BlueprintPure precisely so the widget draws the sectors
the input already uses. Also left: testing an actual throw, tuning 0.55s against the animation, and
deciding whether the wheel should honour the swap lock.
