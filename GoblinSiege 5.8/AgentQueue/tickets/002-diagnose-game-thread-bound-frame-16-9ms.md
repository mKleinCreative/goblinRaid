---
id: 002
title: Diagnose game-thread bound frame (16.9ms, GPU idle)
agent: claude-perf
status: review
claimed: 2026-08-05T18:43Z
build: none
waiting_on: a frame capture taken with the editor window FOCUSED
files: 
  - DIAGNOSIS-ONLY-no-files-claimed-yet
---

## Goal

Diagnose game-thread bound frame (16.9ms, GPU idle)

## Generate

No files edited, no build, no assets touched. Measurements only, in PIE on L_Tutorial_Island:

- `PerformanceService.frame_timing()` before PIE, during PIE, and again after disabling throttle
  (`Slate.bAllowThrottling 0`, `t.MaxFPS 0`, `r.VSync 0`).
- Actor census via `GameplayStatics.get_all_actors_of_class` plus a per-class histogram.
- Tick census two ways: the `primary_actor_tick.start_with_tick_enabled` property, and
  `is_actor_tick_enabled()`.
- Four `stat dumpframe -root=gamethread` captures, read back from `Saved/Logs/MyProject_2.log`.

## Evaluate

**The claim that opened this ticket is NOT SUPPORTED. Do not act on it.**

I reported "game-thread bound, 16.9ms, GPU idle" and proposed cutting Tick/AI cost. The frame
dumps contradict that diagnosis:

```
Frame 45557:  FrameTime 333ms   Game thread tick wait 310.4ms   World Tick Time 9.30ms
Frame 45567:  FrameTime 333ms   Game thread tick wait 305.8ms   World Tick Time 9.80ms
Frame 45577:  FrameTime 333ms   Game thread tick wait 287.7ms   World Tick Time 9.21ms
```

333ms/frame is ~3 FPS and 287-310ms of it is `Game thread tick wait time` - idle. That is the
editor throttling because its window is not focused while a script drives it. Actual world tick
work is ~9.2-9.8ms, INSIDE the 16.67ms budget. `frame_timing()` still reports `bound: GameThread`
because its `game_thread_ms` equals `frame_ms` exactly - it counts the idle wait as game-thread
time. Under a throttled editor that verdict is an artifact, and it survived even after
`Slate.bAllowThrottling 0`.

**What IS established:**

- **It is not Tick or AI cost.** Only **19 actors** have `is_actor_tick_enabled()` true - three
  guards, three AI controllers, three windmill sails, a water wheel, the level BP, GameState, nav
  data. My first census said 8,034 ticking; that was wrong, it read `start_with_tick_enabled`,
  which is true even when `can_ever_tick` is false. The corrected number kills the "reduce ticking
  actors" lever the tool's own hint suggested.
- **Actor census:** 9,076 actors - 8,034 `StaticMeshActor`, 476 `DecalActor`, 259 `GroupActor`,
  201 `SkeletalMeshActor`, 30 `Emitter`. None of the static meshes tick.
- **The render thread is closest to its budget** of anything measured that contains real work:
  15.4ms against 16.67, GPU 10.93ms. 8,034 static mesh actors plus 476 decals is a plausible
  draw-call / primitive-gathering load, and it is the lead I would chase next - NOT the game thread.
- The first dumpframe (45394) is contaminated - it is the frame my own Python ran on. It showed
  76ms `FTicker_Tick`, 7.66ms `FXmppModule_Tick`, 4.84ms `GameplayInsights TickVisualizers`,
  16.4ms `PushPhysicsState`. None of those recur in a clean sample and none should be quoted.
  XMPP ticking at all in a single-player slice is worth a later look, but not on this evidence.

**Never run:** any measurement with the editor focused. That is the blocker on a real answer.

**Owes AGENT_STATE.md** a FAILED entry: performance cannot be measured by driving PIE from
`gs_run.ps1` with the editor in the background - it throttles to ~3 FPS and `frame_timing()`
reports the idle as game-thread time, producing a confident and wrong "GameThread bound" verdict.
I fell for it twice in one session before the dumpframes exposed it.

## Refine

Corrected in response to my own evaluation: withdrew the game-thread conclusion and the
"cut Tick/AI" recommendation outright, and re-ran the tick census with a second method after the
first result failed the smell test - `StaticMeshActor` does not tick by default, so 8,034 ticking
was not credible.

Deliberately left undone rather than guessed at:

- **A clean capture.** The right instrument is `PerformanceService.start_trace` -> Michael plays
  with the window focused for ~20s -> `stop_trace` -> `analyse("both")`. I cannot supply a focused
  window, and every number gathered without one is contaminated the same way.
- **Chasing the 8,034 static meshes.** Merging them or converting to instanced statics is the
  obvious lever IF the render thread is the real bound, but committing to that on a contaminated
  measurement is how a day gets spent optimising the wrong thread.

`Slate.bAllowThrottling 0`, `t.MaxFPS 0` and `r.VSync 0` were set on a PIE session that has since
ended, so nothing persists from them.
