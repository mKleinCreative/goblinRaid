---
id: 332
title: World corruption handoff: fold stages 0-3 into AGENT_STATE so the next session does not rediscover them
agent: claude-corruption
status: done
claimed: 2026-08-27T22:28Z
build: none
waiting_on:
evaluated: 2026-08-27T22:30:38Z
observed: 2026-08-27T22:30:30Z | Read the handoff back from AGENT_STATE and confirmed it answers the questions a cold session would ask - what is built, which two numbers are deliberately unset and why, and which traps in stages 4-6 will silently no-op. It also caught the stale 2026-08-23 icebox note that still described this live feature as parked
scenario: Re-reading AGENT_STATE.md as a fresh session would at run start
files: 
  - AGENT_STATE.md
---

## Goal

World corruption handoff: fold stages 0-3 into AGENT_STATE so the next session does not rediscover them

## Generate

`AGENT_STATE.md` - the handoff, in the three sections a fresh session actually loads.

- **BUILT**: world corruption stages 0-3 (#296/#309/#312/#315/#316/#320/#325/#327), all watched.
  The subsystem shape and why, all five drivers, the director's found-or-spawned rule, the two-ended
  grade, `AGSGameMode::OnCharacterKilled` and the above-both-early-returns placement that makes it
  work, and ruling 62's civilian weighting with the observed 2 soldiers + 1 civilian = 4.5.
- **NEXT**: stages 4-6 with their traps stated up front - the two knowingly unfounded numbers
  (`KillSoftKnee`, `CivilianWeight`), the hand-authored `M_GS_Crop_Master` node, the unknown Niagara
  user-parameter names, the `check_gdd.py` pairing rule, four small debts, one open design question,
  and what is deferred by ruling rather than forgotten.
- **FAILED**: four lessons, each phrased as the rule rather than the anecdote.

Also **superseded the 2026-08-23 icebox note**. It still said #252 was parked pending *"do civilian
kills corrupt the world as much as knight kills?"* - a question that has since been answered (ruling
62) and shipped. A parked-work note that outlives the parking is how the next session concludes a
live feature is dead.

## Evaluate

**Nothing to compile; this is documentation.** The observation for it is that a fresh session can
start stage 4 from `AGENT_STATE.md` alone - which is not verifiable until someone does.

**What I checked rather than assumed:** that `AGENT_STATE.md` was unclaimed (`check -Files` said
CLEAR before the claim), and that the icebox note existed and said what I thought. It also answered
a loose end I had been carrying: **#252 was not abandoned by accident.** Michael iceboxed it
deliberately on 2026-08-23 so it would stop holding the build gate shut, with nothing reverted. My
earlier reading - that it was bookkeeping on a session that looked dead - was wrong in its reason,
though the repair (#296, re-pointing the GDD citation at a live ticket) was right either way.

**Adversarially:**

- **A handoff is only as good as the reader's willingness to read it**, and this one is long. The
  BUILT entry earns its length; the NEXT section is close to the limit of what anyone reads before
  starting work.
- **`AgentQueue/ICEBOX.md` still describes #252 as parked** and I did not claim or edit it. It is now
  the second stale pointer at this feature. Flagged, not fixed.
- **The FAILED lessons are mine, drawn from four fixes in two days.** Whether they generalise beyond
  this feature is a claim I am making, not one I have evidence for.

## Refine

**Changed from my own review:** the FAILED entries were first written as narratives ("the sky went
orange but the extremes stayed blue..."). Rewritten so each leads with the RULE - *print the number,
not the state*; *a multiplier cannot lift a zero baseline*; *test on the map with the most objects* -
because the anecdote is what the next agent skims past.

**Deliberately left undone:** `ICEBOX.md`, per the second bullet above.
