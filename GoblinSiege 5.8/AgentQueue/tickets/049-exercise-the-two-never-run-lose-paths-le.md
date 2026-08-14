---
id: 049
title: Exercise the two never-run lose paths: LeftBehind and OutOfLives
agent: claude-raid2
status: done
claimed: 2026-08-06T21:45Z
build: none
waiting_on:
evaluated: 2026-08-06T21:53:06Z
files: 
  - GoblinSiege 5.8/AGENT_STATE.md
  - decision-queue-kanban.html
---

## Goal

Exercise the two never-run lose paths: LeftBehind and OutOfLives

## Generate

`AGENT_STATE` has carried "LeftBehind and OutOfLives are wired and neither has been exercised; only
Extracted is verified" since #009. Drove both in PIE over MCP, using the debug commands #018 built
for exactly this.

**OutOfLives** - `GS.Raid.SetLives 1` then `GS.Raid.Kill`:
```
[GoblinSiege] lives=5
[GoblinSiege] GS.Raid.SetLives -> 1
[GoblinSiege] GS.Raid.Kill: BP_GSPlayerCharacter_C_0 (hp 100 -> 0)
[GoblinSiege] RAID ENDED: OutOfLives (0/4 objective types burned).
```

**LeftBehind** - fresh PIE (EndRaid is first-call-wins), then `GS.Raid.ExpireClock` **twice**:
```
GS.Raid.ExpireClock: phase=1 raid=0.05s      -> clock=3  raid=0s  collapse=71s
GS.Raid.ExpireClock: phase=3 collapse=0.05s
[GoblinSiege] RAID ENDED: LeftBehind (0/4 objective types burned).
```

**Both paths work.** The transitions are real - neither command sets a result directly; `ExpireClock`
moves the counter and lets `TickRaidClock` decide, `Kill` zeroes the Health attribute.

Also learned: **clock expiry does not strand you, it starts a 90s collapse.** Phase 1 -> phase 3 on
the first expiry, `LeftBehind` only on the second. A two-stage transition, not one.

## Evaluate

**The paths fire. That is the good news and it is the smaller half of the finding.**

**Nothing happens when a raid ends.** After `OutOfLives`:
- `GS.Raid.Status` twice, ~8s apart: `raid=1769s` then `raid=1762s`. **The clock is still running
  after the raid has ended.**
- `lives=0`, `raid ended=yes`, pawn still possessed, `is_game_paused = False`.

Traced it: `EndRaid` sets the result, logs, and broadcasts `OnRaidEnded`. The only subscriber is
`UGSPlayerHUDWidget::HandleRaidEnded`, which forwards to the `BlueprintImplementableEvent`
`OnRaidEnded` - and `WBP_GSPlayerHUD` **does not implement it**. Checked with
`list_overridable_functions`: `OnRaidEnded`, `OnLivesChanged`, `OnObjectiveListChanged`,
`OnRaidClockPhaseChanged` and `OnAlarmPhaseChanged` are **all** `implemented=False`.

So the C++ side of the raid loop is complete and correct, and **every one of its five outputs
terminates in an unimplemented Blueprint event.** A raid ending produces one line in the log.

**`EndRaid` also does not stop the clock.** `LeftBehind` appears to, but only because the clock
expiring is what ended it (`clock=4` after). End for any other reason and it keeps ticking - which is
what the `OutOfLives` run shows.

**Verified by evidence, not inference:** every claim above is a log line or a property read. What I
did NOT do is look at the screen - I cannot capture PIE (unfocused editor renders black, #048), so
"no end screen appears" is inferred from `implemented=False`, not observed. It is a strong inference;
it is still an inference.

**Not tested:** the third result, `Extracted`, still works (verified in #009 and not re-checked
today); whether a raid can be restarted after ending; whether the OutOfLives clock continuing to run
can reach zero and attempt a second `EndRaid` (it would be refused by first-call-wins, so harmless,
but it means the collapse phase can start *after* you have already lost).

**Owed AGENT_STATE.md** - written: four entries covering the dead HUD events, the unstopped clock,
the two-stage clock expiry, and `load_asset` returning None during PIE.

## Refine

- **Ran the second expiry instead of filing "LeftBehind does not fire".** The first `ExpireClock`
  produced a collapse timer and no ending, which reads exactly like a broken path. Reading the status
  line - `clock=3 collapse=71s` - showed it was a designed two-stage transition. One more command
  turned a false bug report into a documented behaviour.
- **Restarted PIE between the two tests.** `EndRaid` is first-call-wins by design, so testing
  `LeftBehind` in the session that had already ended `OutOfLives` would have silently proven nothing.
- **Checked the HUD's overrides rather than concluding the delegate was unbound.** The chain LOOKED
  broken at `EndRaid`; it is intact all the way to the Blueprint and stops there. Those are different
  bugs with different fixes, and only one of them is in C++.
- **Retried `load_asset` after stopping PIE.** During PIE it returned None for an asset that exists
  on disk and was live in the viewport at that moment - which, taken at face value, would have meant
  "the HUD Blueprint is missing".
- **Did not fix anything.** The finding is that five Blueprint events are unimplemented and the clock
  is not stopped. What a lose screen looks like is explicitly not a C++ decision (the header says so),
  and stopping the clock is a one-line change I could make but which belongs with whatever ends up
  handling the end-of-raid state.

**Deliberately left undone:** implementing the five HUD events; stopping the clock in `EndRaid`;
restart-after-loss; and re-verifying `Extracted`.
