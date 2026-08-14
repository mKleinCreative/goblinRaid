---
id: 061
title: Finish the remap: E to IA_Traverse, assign JumpAction, set jump distance tuning
agent: claude-input
status: done
claimed: 2026-08-07T05:41Z
build: none
waiting_on:
evaluated: 2026-08-07T06:05:45Z
files: 
  - Content/Input/IMC_Default.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Finish the remap: E to IA_Traverse, assign JumpAction, set jump distance tuning

## Generate

The three editor edits that made #058's remap and #060's binding actually reachable. Run as a
5-agent workflow: 2 parallel investigators, 1 serial applier, 2 adversarial verifiers.

| edit | before | after |
|---|---|---|
| IMC_Default E row | IA_Climb | **IA_Traverse** |
| BP CDO `JumpAction` | UNSET | **IA_Jump** |
| CharacterMovement `JumpZVelocity` | 490.5 | **620** |
| CharacterMovement `AirControl` | 0.05 | **0.35** |

All four verified off a fresh `collect_garbage()` + reload, not off the objects that were written.

**Repointing E to `IA_Traverse` restored climb, vault AND mantle in one data edit**, because the
Blueprint already routes tap and hold off that action. The `IA_Climb` asset created in #058 was
unnecessary; it is now referenced by nothing and can be deleted.

**Two questions answered that had been open since an investigator died mid-run in #058:**

1. **The goblin has a complete airborne pose.** `ThirdPerson_AnimBP_Gob` has
   JumpStart -> JumpLoop -> JumpEnd, driven by an `IsInAir?` bool set from
   `CharacterMovement->IsFalling()` in `Event BlueprintUpdateAnimation`. So `ACharacter::Jump` sets
   MOVE_Falling, the bool flips, and the state machine enters unaided - no anim work needed.
   Caveat: there is no separate landing state, `JumpEnd` is the whole landing.
2. **Nothing in the Blueprint competes with `ACharacter::Jump`.** All 556 EventGraph nodes were
   enumerated: zero Jump/StopJumping/LaunchCharacter calls, zero `bPressedJump` writes, no IA_Jump
   node. The single `SetMovementMode(MOVE_Falling)` is eight hops downstream of `ClimbDropOff` -
   the climb releasing the wall, not a jump handler.

`IA_Traverse` carries an `InputTriggerHold` with `HoldTimeThreshold = 0.18s` - that is the tap-vault
vs hold-climb split, now living on E.

## Evaluate

**Verified by reading the values back off a fresh load** - E=IA_Traverse, SpaceBar=IA_Jump,
JumpAction=IA_Jump, JumpZVelocity=620.0, AirControl=0.35, and RMB/F/G/LeftAlt intact at
aim/interact/block/dodge. 17 rows, nothing dropped.

**Not verified by play.** Michael tested afterwards and reported the jump lands floaty and the climb
cannot pass a rooftop lip - both real, both now #062. So "the values are correct" and "the movement
is right" are different claims and only the first is proven here.

**Interact is STILL orphaned and this ticket did not cause it.** `InteractAction` on the CDO is
UNSET and has been since it was written - F does nothing, exactly as E did nothing before. Reported
rather than fixed because it is a pre-existing gap with its own scope (the interact framework has
never run at all, per AGENT_STATE).

**A queue conflict I caused.** I left this ticket open after the workflow finished and then tried to
claim the same Blueprint under #062. `gsqueue claim` refused and named this ticket. That is the
protocol working, but the cause was mine: closing a ticket is part of finishing the work, and I
treated the workflow returning as the end.

**Owed AGENT_STATE.md** - DECISION: BP_GSPlayerCharacter overrides `JumpZVelocity` and `AirControl`,
so movement feel is NOT tunable from C++ on this project; and the AnimBP airborne states exist and
are driven by `IsFalling()`, so no anim work is needed for jumps.

**Touched outside the goal:** none; both files claimed.

## Refine

- **Fixed the workflow guard that failed in #058.** That run checked for TOTAL discovery failure and
  let the apply phase proceed when two of three investigators returned null. This run guards each
  input separately and aborts before touching anything if either is missing. The guard did not fire -
  but the previous run only produced a correct result by luck, and luck is not a design.
- **Pointed E at `IA_Traverse` rather than binding `IA_Climb` in C++.** The new action would have
  needed a new UPROPERTY, a new build, and new handlers to do exactly what the Blueprint already
  does off `IA_Traverse`. One data edit beat a build.
- **Aimed the second verifier at reachability, not values.** It traced every key to a handler and was
  told that a hidden orphaned verb must fail it. Checking "is E now IA_Traverse" would have passed
  even if the Blueprint's listener node had gone.
- **Left `IA_Climb` in place rather than deleting it mid-session.** It is unreferenced and safe to
  remove, but deleting an asset is not reversible with `git checkout` the way a property edit is.

**Deliberately left undone:** deleting `IA_Climb`; `InteractAction`; the hold-LeftAlt-plus-direction
dodge; jump-into-climb; and tuning `HoldTimeThreshold` (0.18s), which now governs tap-vault vs
hold-climb on a dedicated wall key and may want a different value there.
