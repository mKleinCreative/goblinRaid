---
id: 172
title: A completed interaction leaves no trace: log the channel lifecycle
agent: claude-channelring
status: done
claimed: 2026-08-17T23:18Z
build: none
waiting_on:
evaluated: 2026-08-17T23:20:08Z
observed: 2026-08-18T01:31:10Z | The log now reports every completed interaction by verb and target - 'Interaction COMPLETED: verb Interact.Loot on BP_LootChest_C_0', the pig's pick up and put down, and the crate - which immediately answered a question that had been unanswerable twice: the chest and the crate DO loot, and had been working all along. The authority suffix stayed absent, confirming these were server-side completions rather than cosmetic client ones.
scenario: PIE in L_CombatArena, four interactions in one session (chest loot, pig carry, pig put-down, crate loot) read back from Saved/Logs/MyProject.log
files: 
  - Source/GoblinSiege/Interaction/GSInteractionComponent.cpp
---

## Goal

A completed interaction leaves no trace: log the channel lifecycle

## Generate

`UGSInteractionComponent` contained **zero `UE_LOG` calls** - no log category, nothing. Added
`LogGSInteract` and two lines:

- **`CompleteChannel`** - `Log` level. Names the verb tag, the target actor, whether it was a pick-up
  or a put-down, and **whether the completing copy had authority**. That last field is the point: a
  client-only completion means the player watched a bar fill while the world did not change, which is
  precisely the failure a naive "interaction completed" line would hide.
- **`AbortChannel`** - `Verbose`. Names the reason enum and the progress percentage at the moment it
  died, so "holding F did nothing" resolves to *which* gate closed - released early, walked out of
  range, turned away, target claimed by someone else. Verbose because releasing early is routine and
  would otherwise drown the completions.

`ActiveVerbTag` is captured into a local in `CompleteChannel` before `ClearChannelState()` wipes it.

## Evaluate

**Written, not yet compiled** - this goes in the same build as #169.

**Why this is a fix and not housekeeping.** The same blind spot has now cost two debugging cycles on
consecutive tickets: the chest in #163 was *reported broken while working correctly*, and the crate in
#168 ended with "wasn't able to tell if holding F on it worked." In both cases the interaction almost
certainly succeeded and there was no way, from inside the game or from the log afterwards, to know. A
system with no runtime readout gets debugged by guessing.

**Not a substitute for #169.** These two are complementary and it is worth being explicit: the ring
tells the *player* what is happening while it happens; the log tells *us* what happened after the fact.
The log alone would leave the game still feeling like a dead key; the ring alone would leave a
completed-but-unpaid client channel looking identical to a real one.

**Deliberately not logged:** `StartChannel`. `OnChannelStarted` already has a visible consumer as of
#169, and a line per press would be the noisiest event in the file while proving the least - a channel
that starts and silently never finishes is already covered, because it produces neither of the two
lines above.

## Refine

**Changed after self-review:** the abort line was `Log` in the first pass. Demoted to `Verbose` -
releasing the key early is ordinary play, and at `Log` it would have buried the completion lines that
are the actual reason this ticket exists. The instrument has to stay readable to be an instrument.

**Nothing else changed.** The authority flag was in the first draft and survives scrutiny: it is the
one field that distinguishes a real interaction from a cosmetic one, and it costs a branch.

## Evaluate

<!-- REPLACE: judge your own output against the goal, adversarially. What is
verified and by what evidence (a log line, a PIE observation, a compile result -
not "should work"); what is written but has never run; what you touched outside
the goal; the DECISION or FAILED line this owes AGENT_STATE.md. -->

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->
