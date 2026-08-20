---
id: 162
title: Supplement to #161 - the consume fix belongs in GSInteractableComponent, not the caller
agent: claude-interact
status: done
claimed: 2026-08-15T06:36Z
build: none
waiting_on:
evaluated: 2026-08-15T06:37:44Z
observed: UNOBSERVED 2026-08-15T07:14:58Z - Same build as #161. Needs a carryable actor to observe, and none exists yet.
scenario: none - never run
files: 
  - Source/GoblinSiege/Interaction/GSInteractableComponent.cpp
---

## Goal

Supplement to #161 - the consume fix belongs in GSInteractableComponent, not the caller

## Generate

The consume fix from #161, split out because it landed in a file that claim did not cover.

`UGSInteractableComponent::CompleteInteraction` ran `SetAvailable(false)` whenever
`bConsumeOnComplete` was true, and that flag defaults to true. After it, `CanInteract` refuses the
target forever. For a carryable that silently means "can be picked up exactly once" - put a pig down
to fight and it is dead cargo, which removes the drop-and-regrab texture the courier run is built on.

Changed to `bConsumeOnComplete && !bIsCarryable`.

## Evaluate

Not compiled, not observed - see #161, same build. Cannot be observed at all until a carryable
exists in the project, and none does today.

The alternative was reordering the two calls in `UGSInteractionComponent::CompleteChannel` so the
carry started before the consume. Rejected: that fixes one call site and leaves the flag wrong for
every future one, and it puts knowledge of carrying into the interaction driver. Carryable-ness is a
property of the interactable, so the exemption belongs on the interactable.

## Refine

Nothing changed after review. The one-line form survives scrutiny: `bIsCarryable` already exists and
is already the thing that decides whether a carry happens at all, so gating the consume on it adds no
new concept.

**Left undone:** whether a carried object should be interactable BY OTHERS while carried. Today it
stays available, which is probably wrong for a prisoner and irrelevant for a pig. Not worth deciding
before either exists.
