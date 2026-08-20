---
id: 161
title: Unblock the interact framework: loud unset-input log, CarrySocket guard, carry-does-not-consume
agent: claude-interact
status: done
claimed: 2026-08-15T06:34Z
build: none
waiting_on:
evaluated: 2026-08-15T06:37:43Z
observed: UNOBSERVED 2026-08-15T07:14:57Z - Compiled clean but cannot be observed yet: the InteractAction warning only fires while the input is unset (correct outcome is Michael assigns IA_Interact and it never appears), the CarrySocket warning only fires until the socket exists, and the carry-consume fix needs a carryable actor - none exists in the project.
scenario: none - never run
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Interaction/GSCarryComponent.cpp
  - Source/GoblinSiege/Interaction/GSInteractionComponent.cpp
---

## Goal

Unblock the interact framework: loud unset-input log, CarrySocket guard, carry-does-not-consume

## Generate

Step 1 of the interaction plan recorded in AGENT_STATE (#159). Three C++ changes, all of which make
a silent failure loud or stop a silent failure happening.

**1. `GSPlayerCharacter.cpp:454` - an else-branch on the InteractAction guard.** `InteractAction` has
been unset on `BP_GSPlayerCharacter` since the day it was written (#061), so the bind is skipped and
`Input_InteractStart` - the only caller of the interact ability in the codebase - has never fired.
Five systems ride on it. The warning names all five so the next person cannot mistake it for a minor
input problem. This is the fourth unset-InputAction bug in the project (#060 JumpAction, #061
InteractAction, #141 HordeOrderAction, now this one), and `HordeOrderAction` directly above already
had exactly this else-branch - the pattern was there, unapplied.

**2. `GSCarryComponent.cpp:167` - a `DoesSocketExist` guard with a real warning.** `CarrySocket`
exists on **none** of the 25 skeletal meshes under `/Game`, verified by loading each and querying.
`AttachToComponent` with a missing socket attaches at the component ORIGIN and reports success, so
cargo rides at the goblin's feet and reads as a physics or pivot bug. The warning names the socket,
the mesh, the owner and the cargo, and says where to add it (the SKELETON, `GOB_Scout_v2_Skeleton`
at `Spine02`, so player and horde goblin both inherit it). It attaches anyway with `NAME_None` - the
fallback is deliberate so the feature still works while the socket is missing, but it is no longer
silent. `NAME_None` in `CarrySocketName` is treated as "origin on purpose" and does not warn.

**3. The consume fix went to `GSInteractableComponent::CompleteInteraction`, NOT to the caller**
(claimed as #162). `bConsumeOnComplete` defaults true and calls `SetAvailable(false)`, after which
`CanInteract` refuses forever. On a carryable that silently means "pick-up-able exactly once": set a
pig down to fight and it is dead cargo. Fixed as `bConsumeOnComplete && !bIsCarryable`.

## Evaluate

**Not compiled.** The editor is open (2 processes) and #161/#162 hold the gate, so the
`-Force -IgnoreQueue` pair is needed and the agent permission layer refuses it. Michael runs the
build.

**Symbols checked against the source rather than assumed**, which is the specific discipline this
session had to learn twice: `AttachCarried()` takes no parameters and `Object` is a local
`AActor* Object = CarriedActor;` (so `Object->GetName()` is valid); `DoesSocketExist` is on
`USceneComponent`; `bIsCarryable` is declared at `GSInteractableComponent.h:119`; the `UE_LOG` form
matches the `HordeOrderAction` branch twenty lines above.

**Nothing here is observed, and two of the three cannot be observed until Michael does the editor
half.** Change 1's warning only fires while `InteractAction` is unset - so the *correct* outcome is
that he assigns `IA_Interact`, the warning never appears, and interact works for the first time.
Change 2's warning only fires until the socket exists. Change 3 cannot be seen at all until there is
a carryable in the project, and there is currently not one.

**Owed to AGENT_STATE:** the interact framework's first execution is the milestone; until someone
holds F and sees a channel bar, the u=10.0 NEXT item stays open.

## Refine

**Changed after self-review:** the socket guard originally skipped the attach entirely when the
socket was missing. That would have made cargo not attach *at all* - a worse failure than attaching
at the feet, and it would have broken the carry state rather than making it ugly. Now it warns and
falls through, so the feature degrades visibly instead of breaking.

**Also reconsidered:** the consume fix was first written in `CompleteChannel` (reordering the two
calls). Moved into `CompleteInteraction` because reordering fixes one call site while the flag stays
wrong for every future one, and because a crate has no business knowing what a carrier is - the
carryable-ness lives on the interactable, so the decision belongs there too.

**Deliberately left undone:** no motion warping, no camera lock, no AI interact task - the three
things ACF has that we do not (#159). Those are the "borrow piecemeal" step, and they come after the
channel has been seen to work at all.
