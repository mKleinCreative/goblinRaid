---
id: 116
title: Code-review correctness fixes: the recoil punish is unreachable, LoseRadius never takes effect, JumpAction is bound unguarded
agent: claude-correctness
status: done
claimed: 2026-08-10T20:45Z
build: required
waiting_on:
evaluated: 2026-08-10T20:51:59Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.h
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - GoblinSiege 5.8/Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Code-review correctness fixes: the recoil punish is unreachable, LoseRadius never takes effect, JumpAction is bound unguarded

## Generate

Three bugs from the code review of this branch. All three were reachable-by-inspection logic
faults, not style issues.

### 1. The recoil punish was unreachable, and blocking granted brief immunity

`UBTTask_MeleeAttack::ExecuteTask` sets `bTargetIsOpen = TargetCharacter->IsRecoiling()` to buy a
swing out of turn against a target left open by a blocked swing (#087). Thirty lines later the
victim's veto ran `if (!Engagement->CanBeAttacked()) return Failed;` - and `CanBeAttacked()`
returned false on exactly `State.Recoil`. The two conditions are exact opposites, and every
`AGSCharacterBase` gets an engagement component in the base constructor, so **every** path that set
`bTargetIsOpen` then failed the veto. Net effect: the reward for reading an attack was that the
attacker became briefly un-hittable - the precise inverse of Michael's 2026-08-08 ruling that a
blocked swing "opens the attacker up".

Fixed by giving `CanBeAttacked` a `bRecoilCountsAsOpening` parameter (default `false`) rather than
dropping `State.Recoil` from its list. This was deliberate, and the reason is the whole design:

- Dead / HitReact / GuardBroken still veto **absolutely**, so a target killed or staggered by
  someone else *during* its recoil window is not a free hit. A blanket bypass would have allowed
  swinging at a corpse.
- `TryAcquireToken` still passes `false`, so **no new attacker can take a token mid-window**. The
  opening is punished by whoever was already engaged, not by a fresh crowd arriving. The
  anti-pile-on rule the component exists for survives intact; only the punish becomes reachable.

Added a `PUNISH - <target> is recoiling, swinging out of turn` line to `GSAIDebug`, because #087
closed with "the branch remains dead code in practice and nobody has proven it can fire" - this is
the log line that will prove it.

### 2. LoseRadius never took effect; targets were dropped at AcquireRadius

`UBTService_AcquireTarget::TickNode` correctly kept a target out to `LoseRadius` (2000), then
immediately rescanned with `FindNearestHostile`, which rejected **every** candidate past
`AcquireRadius` (1500) - including the incumbent it had just ruled legal. `Found` came back null,
`Found != Target`, and the ledger was released and the target cleared. A goblin stepping from 1400uu
to 1600uu was dropped within one `ReacquireIntervalSeconds` and the defender fell to its idle
branch. Both the documented hysteresis band and the `TargetSwitchHysteresis` discount were dead
beyond 1500uu - exactly where they were meant to start working.

Fixed by admitting the incumbent out to `LoseRadius` and everyone else to `AcquireRadius`. Both the
scan and the caller's keep-or-drop test now use `FMath::Max(LoseRadius, AcquireRadius)`, so the two
cannot disagree about where the band ends if the dials are ever authored out of order (Acquire <
Lose is what makes this hysteresis rather than a wobble).

### 3. JumpAction bound unguarded, three lines under a comment explaining why not to

`HornAction` is wrapped in `if (HornAction)` with a comment naming the two properties that had
already shipped unset - one of which is `JumpAction` in #060 - and then the three
`BindAction(JumpAction, ...)` calls just below were unguarded. `BindAction` does not assert on a
null action in 5.8; it registers a binding that never resolves, so an unset `JumpAction` is a dead
space bar with no log, no error and nothing to search for.

Guarded, plus a `[GS.Input]` warning naming the property and the fix when it is unset - turning the
third occurrence of this bug from a silent dead key into a one-line diagnosis.

## Evaluate

**Verified by evidence:**

- **BUILD SUCCEEDED** - see Refine for the figure. This is the acceptance test: the signature change
  to a `UFUNCTION(BlueprintPure)` had to survive UHT, and both default-argument call sites
  (`GSDebugCommands.cpp:570`, `GSEngagementComponent.cpp:203`) had to keep compiling unchanged.
- All five `CanBeAttacked` sites enumerated by grep and each checked for correct semantics under the
  new default: the debug readout and `TryAcquireToken` both want `false`, and both get it implicitly.
- Design intent confirmed against the source rather than assumed: ticket 087 lines 29 and 42 record
  Michael's ruling and the punish's purpose in his own framing, which is what settled the choice
  between exempting recoil globally and exempting it only for the punish.

**NOT verified - and this is the substance of what is left:**

- **No PIE. Not one of these three fixes has been observed working.** Everything above is an
  argument that the code now does what it was written to do; none of it is an observation that it
  does. Specifically:
  - The punish has *never fired in this project* (#087 closed saying so). The new `PUNISH` log line
    is how to check: block an AI swing and watch for it.
  - The hysteresis fix needs a goblin walked from ~1400uu to ~1700uu while a defender holds it; the
    defender should keep its target instead of dropping to idle.
  - The JumpAction warning only appears if the property is actually unset, which it may not be.
- **The punish now genuinely changes combat feel.** It was dead code; making it live means AI will
  swing out of turn at a recoiling player. #087 called the player side untested even when the branch
  could not fire - that is now more true, not less. If it reads as unfair, the dial is
  `GS.Combat.RecoilSeconds` (0.6), not this logic.
- `AcquireRadius`/`LoseRadius` defaults were not retuned. The band now works as documented for the
  first time, which is itself a behaviour change: defenders will hold targets ~500uu further out
  than they did yesterday.

**Owes AGENT_STATE.md:** a correction line - the #087 punish was never reachable from the day it
shipped, so any earlier observation of "recoil does nothing" was this, not a tuning problem.

## Refine

**Changed in response to my own evaluation:** the first implementation wrapped the whole veto in
`if (!bTargetIsOpen)`. I rejected it on re-reading: it also bypassed the Dead, HitReact and
GuardBroken checks, so a target killed by someone else mid-recoil could still be swung at. Replaced
with the explicit `bRecoilCountsAsOpening` parameter, which exempts recoil only and keeps the tag
knowledge in one place instead of duplicating it into the BT task.

**Deliberately left undone:**

- **No PIE run.** Driving three separate combat scenarios is a session of its own and wants someone
  watching the screen; the two AI fixes both have log lines ready for it.
- **`GSDebugCommands.cpp:570`'s string** still reads `(immune: staggered/recoiling/dead)`, which
  remains accurate for the default call. Not touched - the file was not claimed by this ticket.
- **The remaining eight code-review findings** are untouched and still open: the recoil-clear timer
  handle being a discarded local (`GSCharacterBase.cpp:201`), the per-hit `FReferenceSkeleton` copy
  (`GSArrowProjectile.cpp:176`), `RemoveFromActive`'s stale-entry early return
  (`GSHordeSubsystem.cpp:385`), stamina suppression not stopping drain
  (`GSStaminaComponent.cpp:31`), and the four `level-gen/generate.py` findings. This ticket claimed
  only the three Michael named.
