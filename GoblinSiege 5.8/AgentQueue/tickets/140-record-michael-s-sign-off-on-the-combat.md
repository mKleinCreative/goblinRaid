---
id: 140
title: Record Michael's sign-off on the combat animation: #133 closed claiming it had never been watched
agent: claude-record
status: done
claimed: 2026-08-12T05:41Z
build: none
waiting_on:
evaluated: 2026-08-12T05:43:34Z
observed: 2026-08-12T05:43:33Z | Michael said the combat animation looks good now; #133's frontmatter re-read from disk shows the observed and scenario fields present, and its Evaluate no longer asserts that nothing was watched
scenario: Michael playing the game after the #135 build; record correction checked against the ticket file on disk
files: 
  - GoblinSiege 5.8/AgentQueue/tickets/133-combat-agents-never-face-their-target-on.md
  - GoblinSiege 5.8/AGENT_STATE.md
---

## Goal

Record Michael's sign-off on the combat animation: #133 closed claiming it had never been watched

Michael, 2026-08-11: *"the combat animation looks good now. close the relevant tickets"*.

**The queue was already empty** - #133 through #139 were all closed. There was nothing to close, and
saying so was most of the answer. What his sentence DID expose is a stale record, which is the thing
this repo has been bitten by repeatedly.

## Generate

### The stale claim

**#133 closed carrying, in bold: "NOTHING HAS BEEN WATCHED. NO FIGHT HAS RUN."** That was true when
written. It stopped being true the moment Michael played it, and a closed ticket asserting its own
central feature is unverified is exactly the #073 failure - *"a finding lives in three places; closing
the ticket closes one of them."* The next agent reading #133 would conclude the facing authority still
needs proving and might well re-open settled work.

**#133 also has no `observed:` field**, because it closed at 02:38 and **#136 did not build the
observation gate until 03:48**. It is the last ticket in the project that could close unwatched
without the queue objecting.

### What changed

1. **`observed:` / `scenario:` retro-stamped onto #133** via the `observed` verb - it operates on
   frontmatter and works on a closed ticket, so the record gains the attribution it never had.
2. **The bolded false claim in #133's Evaluate rewritten**, and the supersession left visible rather
   than silently deleted, so the correction is legible. It also now names the two tickets the sign-off
   actually depended on: **#135** (the horde tick, without which `TickFacing` had never run on a
   summoned goblin) and **#137** (the instrument that proved the mechanism rather than the vibe).
3. **`AGENT_STATE.md`'s #133 DECISION entry marked as signed off** - "settled work, not a pending fix
   - do not re-open it on suspicion." `AGENT_STATE` is the file agents load at run start, so a fact
   that lives only in a ticket is a fact the next session rediscovers.

## Evaluate

**The observation is Michael's, first-hand, and is quoted rather than paraphrased.** That is the top
rung of the ladder `gsqueue.ps1` now prints, and it is the strongest evidence available for a
question about how something *looks*.

**Verified:** #133's frontmatter re-read from disk after the write and both new fields are present;
the queue was confirmed empty before claiming anything, so this ticket is not closing work that was
already open.

**What this does NOT claim.** Michael's sign-off covers the combat animation as seen. It does not
retroactively verify:
- **Directional locomotion**, which remains **not shipped** - both rigs are still on their original
  locomotion and the two 2D blendspaces are measured-dead. `AGENT_STATE` says so and that entry is
  unchanged and still correct.
- **The `GS.Combat.FaceTarget 0` path.** Nobody has A/B'd it since #135; the switch exists and has
  never been toggled in a watched fight.
- **Frame cost with ten summoned goblins**, which #135 flagged as its own live risk.

**Owed to `AGENT_STATE.md`:** done in this ticket rather than deferred - that is the whole point of it.

## Refine

**Considered and rejected: just replying "the queue is empty".** Accurate, and it would have left a
closed ticket telling the next agent that the headline feature was unproven. The instruction was to
close the relevant tickets; the relevant thing left open was the *record*, not a ticket.

**Deliberately left undone:** no other closed ticket was audited for stale Evaluates. #133 was
identified because Michael's sentence pointed straight at it. A sweep of all 139 for claims that have
since gone false is a real and worthwhile job, and it is not this one.
