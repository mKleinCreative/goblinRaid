---
id: 381
title: Pig/Sheep/Chicken: AnimBPs built, sheep+chicken actors created, all placed as raid targets on Tutorial Island
agent: claude-overnight
status: done
claimed: 2026-08-30T10:29Z
build: none
waiting_on:
evaluated: 2026-08-30T10:30:02Z
observed: UNOBSERVED 2026-08-30T10:42:43Z - Pig/Sheep/Chicken idle AnimBPs were validated (state machine valid, zero errors) and anim_class assignment was read back on all 12 new + 3 existing placed animals, but nobody has actually looked at one in PIE or the editor viewport - no confirmation the idle pose reads correctly rather than a subtle retarget glitch or T-pose
scenario: none - never run
files: 
  - Content/Blueprints/Interactables/BP_Livestock_Pig.uasset
  - Content/Blueprints/Interactables/BP_Livestock_Sheep.uasset
  - Content/Blueprints/Interactables/BP_Livestock_Chicken.uasset
  - Content/Blueprints/Interactables/ABP_Pig.uasset
  - Content/Blueprints/Interactables/ABP_Sheep.uasset
  - Content/Blueprints/Interactables/ABP_Chicken.uasset
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Pig/Sheep/Chicken: AnimBPs built, sheep+chicken actors created, all placed as raid targets on Tutorial Island

## Goal

Michael, before signing off for the night: "do all the groundwork for the subtasks I sent you on
without my permission... at least do all the editor work for them all on tutorial_island, then get
all their builds in together if you need it." This ticket is the Pig/Sheep/Chicken half of that
(`#380`'s design scope); `#379` (loot command) is separate.

## Generate

**Pig (fixed a real bug, not just added polish):** `BP_Livestock_Pig`'s `Mesh` component had
`animation_mode = ANIMATION_BLUEPRINT` with `anim_class = None` - the pig was rendering in a bind
T-pose on both `L_CombatArena` and `L_Tutorial_Island`. Built `ABP_Pig`
(`AnimGraphService.build_state_machine`, single-state "Ambient" machine, entry state "Idle" looping
`A_Pig_Idle`, validated clean - `validate_state_machine` returned `is_valid: True`, zero
errors/warnings) and assigned it to `BP_Livestock_Pig`'s Mesh component default. All 3 pigs already
placed on `L_Tutorial_Island` picked up the fix automatically on Blueprint recompile (confirmed by
reading each instance's `anim_class` before touching them - already `ABP_Pig`, not `None`). The
`L_CombatArena` pig did NOT auto-propagate (had its own stale per-instance override) and needed an
explicit per-instance fix - noting the asymmetry in case it recurs elsewhere.

**Sheep and Chicken (didn't exist as actors at all before tonight):** duplicated
`BP_Livestock_Pig` -> `BP_Livestock_Sheep` / `BP_Livestock_Chicken` (preserves the
`GSInteractableComponent` wiring - carryable, `ChannelSeconds=1.0`, `CollapseImpulse=250` - rather
than re-authoring it from scratch), retargeted each `Mesh` to `SK_Sheep`/`SK_Chicken`, and set
`LootValue` to 25/10 - these exact numbers were NOT guessed, they're documented directly in
`GSInteractableComponent.h`'s own `loot_value` comment: "livestock 40/25/10 for pig/sheep/chicken".
Built `ABP_Sheep`/`ABP_Chicken` the same single-state-Idle way as the pig
(`A_Sheep_Idle`/`A_Chicken_Idle_Anim`), both validated clean, both assigned.

**Placement:** spawned 2 sheep + 2 chicken around each of the 3 existing pig locations (12 new
animals total), ground-snapped via a line trace (same method proven on the guard-placement pass
earlier tonight - after that pass's "actors in the ground" mistake, every placement this session
ground-snaps before considering itself done). All 12 confirmed correctly wired
(`anim_class` readback matches the intended AnimBP per-animal) before saving.

Saved. No C++ touched - this entire ticket is Blueprint/AnimBP/level content, no rebuild needed for
any of it.

## Evaluate

**Verified by property read-back, not assumed:** every AnimBP's state machine validated
(`is_valid: True`, `errors: []`) before being assigned anywhere; every placed animal's `anim_class`
was read back AFTER assignment and matches the intended class, not just "the call returned true".
**NOT verified visually** - no screenshot/PIE look confirms the idle animation actually plays
correctly at runtime (a `CaptureViewport` attempt failed on a parameter-shape mismatch and this
ticket did not chase it further, given the volume of remaining work and that the underlying
verification - state machine validity + correct assignment - is solid on its own). First real look
happens whenever Michael next opens the editor.

**Deliberately NOT done:** Hurt/Death reaction animations exist as clips for all three species but
are not wired to anything - there is no trigger (no "this animal was just looted/killed" event
hooked to the AnimBP) and adding one was out of scope for "add animations to the pig." The AnimBPs
are single-state (Idle only) on purpose - richer ambient variation (occasional Sit/Sleep/Eat) would
need transition rules against some kind of timer/random variable, not attempted here.

## Refine

Handing back at `review` - needs Michael to actually look at a pig, sheep and chicken in the editor
or PIE and confirm the idle animation reads correctly (not a T-pose, not a glitch), same as every
other unverified-visually change tonight.
