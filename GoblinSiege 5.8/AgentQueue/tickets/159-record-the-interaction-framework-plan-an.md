---
id: 159
title: Record the interaction-framework plan and the ACF comparison in AGENT_STATE
agent: claude-interactplan
status: done
claimed: 2026-08-14T23:42Z
build: none
waiting_on:
evaluated: 2026-08-14T23:44:58Z
observed: UNOBSERVED 2026-08-14T23:44:58Z - A plan written into project memory has no runtime. The plan's own step 1 - assigning IA_Interact and watching a channel fill - is the first thing that would produce evidence, and it has not been done. One ACF claim (no hold-to-channel) rests on three headers and is flagged as needing UACFSustainedAction checked.
scenario: none - never run
files: 
  - AGENT_STATE.md
---

## Goal

Michael asked for a plan of action for the interaction framework, checking what ACF can supply
before anything is built - his standing rule, and one he has had to state twice. He then asked for
it written into AGENT_STATE rather than left in chat.

## Generate

Three edits to `AGENT_STATE.md` (844 -> 915 lines).

**1. A DECISIONS block: the plan and the ACF comparison.** How the framework went missing, what ACF
actually ships (checked in the plugin source, not assumed from the skill pack), the three
capabilities ACF has that we lack, the two we have that it lacks, why adoption is not a drop-in, the
three ordered steps, and two open questions for Michael.

**2. Corrected the 2026-08-06 NEXT-refresh note.** That note is where the framework was lost: it
removed the interact framework from the ranked list - **at u=10.0, the highest-utility item on it** -
because "all four components exist". The note now records that the removal was wrong, and that the
refresh rule which produced it (*remove an item once its `missing:` symbols exist*) is unsafe and
should read *remove it once something has been observed running*.

**3. Restored the item to NEXT at the top**, with the step-1 fix named inline and an explicit
instruction not to remove it again on the strength of symbols existing.

**THE FINDING WORTH KEEPING.** The interact framework was deleted from the work list *because its
classes existed*. It has never executed - `InteractAction` is unset on the player CDO, so the bind at
`GSPlayerCharacter.cpp:454` is skipped and `Input_InteractStart` has never fired; and nothing carries
a `UGSInteractableComponent`, so `ResolveChannelTarget` always returns null. 1409 lines of written,
compiling, replicated code, blocking five other systems, silently off the plan for eight days. This
is precisely the failure the BUILT/WIRED/SKELETON grading exists to prevent, and it landed on the
most load-bearing system in the slice.

**On the ACF question:** ACF ships MORE than we knew - interface, interactable component, pawn
component, a GAS ability with **motion warping to the interactable**, a ready-made base actor, and
`ACFInteractSmartObjectsTask` so **AI can interact** - and it is still not the right adoption today,
because `UACFInteractActionAbility` derives from `UACFActionAbility` and expects an ACF-shaped
interactor. Taking it pulls the character/ability migration into the critical path. Recommendation:
make ours run first (half an hour), then borrow ACF's motion warp, camera lock and AI task piecemeal.

## Evaluate

**Verified by grep and re-read:** DECISIONS block at :474, corrected refresh note at :543, restored
NEXT item at :555 carrying u=10.0. One file modified.

**The ACF claims come from the plugin source, not the skill pack.** Read `ACFInteractionComponent.h`,
`ACFInteractableComponent.h`, `ACFInteractActionAbility.h` and `ACFTypes.h`'s `EACFInteractionState`
directly. The skill pack was the entry point - it named `IACFInteractableInterface` - but every
capability claim in the plan traces to a header. That matters because the claim driving the
recommendation is a claim about an ABSENCE, and absences are exactly what a summary hides.

**NOT verified: the absence claim itself.** I read three headers and the state enum; I did not read
`UACFActionAbility`'s full lifecycle, and `UACFSustainedAction` exists elsewhere in the Actions module
- the actions-system skill describes it as "hold-triggered, released by `ReleaseSustainedAction()`".
**If ACF's sustained action can drive an interaction, the comparison changes materially and possibly
the recommendation with it.** That is the single most valuable thing to check before acting.

**Nothing was run.** No build, no PIE, no editor change. The plan's own step 1 is the first thing
that would produce evidence.

## Refine

**Changed after self-review:** the first draft asserted flatly that ACF cannot express a hold
channel. Softened to name `UACFSustainedAction` as the thing that could refute it - stating an
absence from three headers is exactly the over-claim this ticket is otherwise complaining about. The
recommendation survives either way, because the deciding factor is the coupling to the ACF character
stack rather than the channel, but the reasoning should not rest on a gap I did not exhaustively
search.

**Deliberately left undone:**

- **`UACFSustainedAction` not read.** Named as the open check above.
- **No tickets cut** for the three steps; Michael asked for AGENT_STATE first.
- **BUILT not audited.** The 08-06 refresh removed THREE items by the symbols-exist rule; the other
  two (lives/respawn, runic site) later earned real observations in #009/#049/#051, so they happen to
  be fine. That is luck, not process, and a pass over BUILT for anything promoted on symbol existence
  rather than observation is worth doing.
