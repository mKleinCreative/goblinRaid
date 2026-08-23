---
id: 257
title: Wire T to the Warren placement component: component on the pawn, Started and Completed bindings
agent: claude-warren
status: done
claimed: 2026-08-23T18:37Z
build: none
waiting_on:
evaluated: 2026-08-23T22:42:32Z
observed: 2026-08-23T22:42:45Z | Holding T raised the placement ghost and releasing it planted a Warren, which is the input binding firing end to end.
scenario: PIE in L_CombatArena on the player pawn, editor build of 2026-08-23 15:38.
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Wire T to the Warren placement component: component on the pawn, Started and Completed bindings

## Generate

`GSPlayerCharacter.{h,cpp}` - the wiring #245 deliberately left undone while claude-acf held this file.

- `WarrenPlacementComponent` as a **C++ default subobject**, not a Blueprint-added component, so
  every player has one without anyone remembering to add it. `BP_GSPlayerCharacter` therefore needs
  no edit and was not touched.
- `PlaceWarrenAction` bound **Started -> BeginPlacement, Completed -> ConfirmPlacement**. Two
  bindings rather than a Hold trigger on the action, which is the horn's hard-won precedent: a Hold
  trigger does NOT stop the Started pin firing (#207/#208 cost a session to that).
- Guarded with a loud `else` branch. `BindAction` does not assert on a null action in 5.8 - it
  registers a binding that never fires, and this project has shipped that bug three times.
- Handlers take `const FInputActionValue&`, matching every other input handler here. My first patch
  assumed a no-arg signature and the assertion caught it before anything was written.

## Evaluate

**NOT COMPILED, NOT RUN.** The gate is closed (7 tickets).

**Verified:** the three preconditions for T that were previously false are now addressed in source -
component on the pawn, action bound, handlers implemented. Confirmed by grep, which is a spell-check,
not a compile.

**The remaining gap, and it will look identical to today's failure:** `PlaceWarrenAction` is an unset
`TObjectPtr<UInputAction>` on the CDO. It cannot be assigned from Python until the module is rebuilt,
because the property does not exist in reflection data until then - the same trap `HornMesh` hit on
#242. **So after the build there is one more step**, or T still does nothing and the log says so.

## Refine

Nothing changed on review. Left undone deliberately: assigning `PlaceWarrenAction` on the Blueprint,
which is impossible before the build.

> 2026-08-23T18:39Z Wired in source. Needs a build, then IA_PlaceWarren assigned on the BP CDO.


### Post-build truth, 2026-08-23

**BUILT (dll 15:38) AND WATCHED.** The "NOT COMPILED, NOT RUN" text above was true when written and
is now stale; this section supersedes it rather than leaving the reader misled.

Michael confirmed: **T works** - hold raises the ghost, green on the arena floor, release plants a
Warren - and **portal banking works**.

**Known open, accepted for now:** the ghost colour still flickers. `GhostVisualLift` (8uu) did not
cure it, which points at the placeholder mesh rather than the offset: the engine `Cylinder` has its
pivot at the CENTRE, so roughly half of it is below ground regardless of the lift. The fix is either
a much larger lift or offsetting by half the mesh height - and it disappears entirely once the mouth
has real art and stops being a cylinder. Michael chose to move on.

> 2026-08-23T22:42Z Built 2026-08-23 15:38 and watched.
