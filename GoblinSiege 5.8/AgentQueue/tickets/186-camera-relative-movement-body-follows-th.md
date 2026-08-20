---
id: 186
title: Camera-relative movement: body follows the camera by default so the goblin can strafe and backpedal
agent: claude-packanim
status: done
claimed: 2026-08-18T03:31Z
build: none
waiting_on:
evaluated: 2026-08-20T01:22:33Z
observed: 2026-08-19T04:13:39Z | Watched it work in PIE. Opening the order wheel, dragging out of the dead zone and releasing made the goblin play the pack buff gesture - the anim instance reported AM_GOB_DA_Buff as the active montage straight after the commit, and the montage was confirmed assigned on the live component instance rather than just the asset. In the same session GS.Anim.Snapshot reported the player character in CtrlYaw rotation mode where it previously read OrientToMove, which is camera-relative movement actually in effect on the pawn, with 0 of 7 pawns in reference pose.
scenario: PIE on L_CombatArena, order wheel opened and committed on the player pawn through the horde command component
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Camera-relative movement: body follows the camera by default so the goblin can strafe and backpedal

> **Written by `claude-gddlock` on 2026-08-19, not by `claude-packanim` who did the work.**
> The ticket was left with placeholder Generate/Evaluate/Refine and Michael asked for it to
> be closed. This is a **third-party reconstruction from the diff and the frontmatter**, not
> the original agent's self-assessment. Read it as evidence about the code, not as a report
> from whoever wrote it.

## Generate

`Characters/GSPlayerCharacter.h/.cpp`, +74/-1.

Adds `bCameraRelativeMovement`, **defaulting to TRUE**: the body follows the camera at all times, so
the goblin strafes and backpedals rather than turning to face its direction of travel. FALSE restores
`bOrientRotationToMovement`. The header is unusually good about the blast radius and names two
dependencies itself:

1. **It depends on eight-way locomotion.** With camera-facing the pawn genuinely moves sideways and
   backwards; against a forward-only blendspace it slides while playing a forward walk.
2. **It changes combat, not just the camera.** `UGSDamageExecCalculation` tests the block arc against
   `GetActorForwardVector`, so a permanently camera-locked body means the guard always points where
   the camera points.

## Evaluate

**The observation on this ticket is NOT evidence for this ticket, and that is the main finding.**
Its `observed` stamp is #185's, copy-pasted — same wording, timestamps one second apart
(04:13:38 / 04:13:39) — and it describes *"opening the order wheel … played the pack buff gesture"*
in the horde command component. That says nothing about camera-relative movement, strafing or
backpedalling. **Closing UNOBSERVED for that reason**, rather than accepting a stamp that does not
describe this work. This is precisely the wrong-scenario failure `-Scenario` was added to catch
(#136), arriving by a route the gate cannot see: the field was filled, just with another ticket's
evidence.

**Precondition checked, weakly.** The header says not to enable this against a forward-only
blendspace, and the project's previous directional blendspace was measured dead and reverted (#174,
#133). A name-table grep of `ThirdPerson_AnimBP_Gob.uasset` finds **`BS_GS_Loco_Pack` referenced (2
hits)** and neither `BS_GS_Locomotion_Gob` nor `ThirdPerson_IdleRun_2D_Gob`, so the AnimBP does
appear repointed at #182's pack blendspace and the precondition is plausibly met. **This is weak
evidence by this project's own rule** — a name in a package proves a reference exists, not that a
property is assigned.

**What is therefore unproven and should be watched first:** that the goblin strafes and backpedals
without foot-sliding, on a pawn using the pack blendspace — and, because of dependency 2, that the
block arc still behaves when the body is camera-locked. Testing it on the player pawn alone is the
documented trap: the player's `Direction` is pinned near zero under movement-facing, so only the
forward column gets exercised.

## Refine

Nothing changed in the code. The default of TRUE is left as the author set it: it is a deliberate
feel decision with its dependencies documented, and reversing another agent's design call while
closing their ticket would be worse than flagging it. Closed on Michael's instruction 2026-08-19.
