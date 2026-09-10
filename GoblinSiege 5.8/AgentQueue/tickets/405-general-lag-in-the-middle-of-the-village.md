---
id: 405
title: General lag in the middle of the village during a raid
agent: claude-villagelag
status: done
claimed: 2026-09-09T19:31Z
build: none
waiting_on:
evaluated: 2026-09-10T00:02:14Z
observed: 2026-09-10T00:01:45Z | Michael captured 2374 frames after the shadow change and the pass it targeted collapsed: GPU/ShadowDepths 21.84ms to 2.76ms, GPU time 28.47 to 10.89, and the median frame 33.25ms to 21.77ms. p95 frame went 79.6 to 62.8ms. The frame is now game-thread bound instead of GPU bound
scenario: Live PIE raid played by Michael in a focused window with r.GPUCsvStatsEnabled 1 and csvprofile running, compared against his own 3974-frame capture from before the change
files: 
  - none-investigation-only
---

## Goal

Michael's long-standing item: "general lag around the middle of the village during a full raid loop,
likely village density rather than any single system". Then, mid-investigation: "is there a way for
you to check any of the advanced tracing features in Unreal engine to help you find major issues".

## Generate

One code change, and a measurement method that should have been used from the start.

**`AGSBuildingObjective::MaxGlobalRubbleFX` (24)** - a map-wide ceiling on simultaneous rubble-dust
bursts, alongside the existing per-building `MaxRubbleFXPerBuilding` (6). A per-building cap is not a
cap when 67 buildings collapse together: 67 x 6 is ~400 permitted, and a razed village measured
**141 simultaneous `N_PebbleDust` instances costing 8.3 ms of game thread, 7.98 ms of it Niagara
particle COLLISION**. Registry of weak pointers that self-prunes as bursts auto-destroy, same shape
as `UGSBurnFXComponent::MaxGlobalSmolderFX`.

This is the third instance of one pattern in this module: a per-piece Niagara system with a
per-owner cap that does nothing at map scale. #395 found it in the smolder FX, #400 found the
spacing half of it, and this is the dust. **Any future per-piece Niagara system wants a global budget
before it ships.**

## Evaluate

**The headline finding is not the dust. It is that EVERY perf number taken in this session before
the throttle was found is wrong**, including ones already reported to Michael as real.

The editor throttles PIE when its window is not focused, and this whole session drives the editor
over HTTP with the window unfocused. Measured back to back:

| | throttled | unthrottled |
|---|---|---|
| game thread | 55.6 ms | **22.3 ms** |
| fps | 18.0 | **44.8** |
| render / gpu | 0.0 / 7.2 | 11.9 / 13.8 |

`render=0.0` and a FrameTime of exactly 333.33 ms (3 FPS) are its signature. **#402's "84 ms after
freezing" and "45.6 ms" were measured under it and are inflated.** The before/after DELTA there
survives - both sides were throttled identically and 1381 -> 84 is far too large to be an artifact -
but the absolute numbers should not be quoted.

Setting `bThrottleCPUWhenNotForeground=False` in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`
did NOT take; a `FrameTime` of 333.33 ms persisted afterwards. The per-measurement console command
(`Slate.bAllowThrottling 0`) does work, and is what the unthrottled figures above came from.

**Unthrottled, the shape of the problem is:**

| state | game thread | fps |
|---|---|---|
| village, fires burning | 20.8 ms | 48 |
| peak of a 67-building mass collapse | 1367.8 ms | 0.7 |
| after everything settles and freezes | 46.2 ms | 21.7 |

So the burning is fine. **The lag is the RAZED state**, which nothing had looked at because
everyone was watching the collapse.

**CSV profiler over 80 frames** (`csvprofile start`/`stop` -> `Saved/Profiling/CSV/`), which is the
answer to Michael's question and what should have been used instead of `stat dumpframe`, whose
single frames misled twice:

| | median ms |
|---|---|
| Exclusive/AllWorkers/Effects | **93.88** |
| GameThreadTime | 51.93 |
| Exclusive/GameThread/Effects | **18.07** |
| Exclusive/GameThread/UI | 5.17 |
| Exclusive/GameThread/CharacterMovement | 4.03 |
| Exclusive/GameThread/TickActors | 2.75 |

**Niagara is the cost.** 18 ms of game thread and 94 ms across workers, against 2.75 ms for ticking
every actor in the level. "Village density" was a reasonable guess and it is not what the profile
says; gameplay ticking is nearly free by comparison.

Counters from the same capture, for whoever picks up the rendering half:
`RHI/PrimitivesDrawn` **6,699,711**, `SceneCulling/NumStaticInstances` 455,860, `RHI/DrawCalls` 1,497,
`ActorCount/TotalActorCount` 16,621 (13,781 of them StaticMeshActor), `ActorCount/DecalActor` 952.

**Not verified:** the dust cap's effect on the frame. The post-build measurement came back
`render=0.0`, i.e. throttled, so it was discarded rather than reported. The cap is a code change with
a measured justification, not a measured improvement.

## Refine

Next, in the order the profile argues for:

1. **The remaining Effects cost.** The dust was 8.3 ms of an 18 ms game-thread Effects budget. The
   rest is the fire systems - a burning village held 24 `NS_Fire_Big`, 24 `NS_FlameEmbers` and 13
   `NS_GS_SurfaceFire` simultaneously. `AGSBuildingObjective::MaxFireFX` is per-building, exactly
   like the dust cap was. That is the obvious next global budget.
2. **Particle collision specifically.** 7.98 of the dust's 8.3 ms was collision, and #395 measured
   the same for smolder. If the fire systems also collide, disabling it on systems that do not need
   it is worth more than any count cap - but it is an asset edit and Michael's call.
3. **The rendering half.** 6.7 M primitives and 456 K static instances per frame is a separate
   investigation, and the razed state is where to look: 366 geometry collections' worth of wreck are
   still fully rendered and shadow-casting after the freeze. `CastShadow=false` on frozen wrecks is
   the cheap first thing to try, and `UGSCrumbleComponent::FreezeSettledPhysics` is already the
   natural place to do it.

**Method note for the next agent:** use `csvprofile start`/`stop` and read the CSV, not
`stat dumpframe`. A single frame told me `FTicker_Tick` was 44.8 ms (it was the throttle's idle) and
told me nothing about Effects, which the 80-frame median put at the top immediately. And disable
throttling with the console command in the same script as the measurement - the ini setting does not
hold.

---

## Michael's own capture, 2026-09-09 16:08 - 1376 frames, unthrottled, real play

This supersedes every measurement above. His PIE window is focused, so no editor throttle, and
1376 frames of actual play beats anything driven over HTTP.

**The effects caps worked.**

| | my throttled capture | his real capture |
|---|---|---|
| `Exclusive/GameThread/Effects` | 18.07 ms | **0.82 ms** |
| `Exclusive/AllWorkers/Effects` | 93.88 ms | **8.98 ms** |

**But the frame is still over budget, and the reason is NOT what this ticket concluded.**

| | median | p95 |
|---|---|---|
| FrameTime | 23.92 ms | 74.93 ms |
| GameThreadTime | 23.89 ms | 61.68 ms |
| RenderThreadTime | 12.23 ms | 66.70 ms |
| GPUTime | 13.29 ms | 65.69 ms |

Two separate problems, and they need separate fixes:

**1. The median (24 ms, ~42 FPS) is game-thread bound.** Top items now that Effects is gone:
`UI 4.61`, `Animation 3.31`, `TickActors 2.24`, `CharacterMovement 1.87`. **UI is the single
largest game-thread cost in the game** and nobody has ever looked at it.

**2. The SPIKES (p95 75 ms) are GPU-bound, not CPU.** Worst 60 frames (median 90.4 ms) against all
others (23.7 ms):

| stat | spike | normal | delta |
|---|---|---|---|
| RenderThreadTime | 73.05 | 12.19 | **+60.86** |
| GPUTime | 70.12 | 13.16 | **+56.96** |
| RenderThread/EventWait | 59.73 | 12.36 | +47.38 |
| GameThread/EventWait | 39.55 | 0.01 | +39.53 |
| GameThreadTime | 29.16 | 23.62 | **+5.54** |

The game thread barely moves. It is WAITING 39.5 ms for a render thread that is waiting on a GPU
that has jumped from 13 ms to 70 ms. **The stutter is the GPU.** This ticket's "game-thread bound"
conclusion came from throttled data and is withdrawn for the spike case.

`DrawCall/Translucency` is 55 normally and 69.5 in spikes - translucency is smoke and fire, so the
FX are still implicated, just on the GPU rather than the CPU. Not conclusive: the capture has no
GPU pass breakdown.

**Next capture needs `r.CsvGpuStats 1` set before `csvprofile start`.** That adds per-pass `GPU/`
columns and would name the expensive pass directly instead of inferring it from a draw-call count.

Counters from his capture, for the rendering work: `RHI/PrimitivesDrawn` 2,592,306,
`RHI/DrawCalls` 5,750, `SceneCulling/NumStaticInstances` 625,947, `ActorCount/TotalActorCount`
9,360, `ActorCount/DecalActor` 476.

---

## VERIFIED, 2026-09-09 17:00 - Michael's before/after captures

`GPU/ShadowDepths` was 77% of the GPU frame. Turning shadow casting off on settled wrecks
(`bDisableShadowsOnSettle`, in `UGSCrumbleComponent::FreezeSettledPhysics`) cut it by 87%.

| median | before (3974 fr) | after (2374 fr) | delta |
|---|---|---|---|
| GPU/ShadowDepths | 21.84 | **2.76** | -19.08 |
| GPUTime | 28.47 | 10.89 | -17.58 |
| RenderThreadTime | 25.21 | 10.96 | -14.24 |
| **FrameTime** | 33.25 | **21.77** | **-11.48** |
| Exclusive/AllWorkers/Effects | 18.59 | 8.50 | -10.09 |

p95 FrameTime 79.64 -> 62.83, p95 GPUTime 61.84 -> 38.73, p95 ShadowDepths 53.83 -> 27.42.

**Caveat:** two different play sessions, not a controlled A/B, so magnitudes carry noise. A 19 ms
drop in exactly the targeted pass is not noise, but do not quote these to three significant figures.

**A detail worth keeping:** `DrawCall/ShadowDepths` barely moved (1766 -> 1717) while the cost
collapsed. It was never the NUMBER of shadow draws - it was the geometry they pushed, ~45,000 wreck
pieces in the shadow map. A draw-call count would have said this change did nothing.

### Where the frame stands now

**Median 21.77 ms (46 FPS), and the bound has flipped from GPU to game thread** (GT 21.85 vs GPU
10.89). Game-thread breakdown:

| | ms |
|---|---|
| **UI** | **4.60** |
| Animation | 2.65 |
| TickActors | 1.81 |
| CharacterMovement | 1.67 |
| Effects | 0.89 |

Those sum to ~12 ms of 21.85, so roughly 8 ms is untracked and would need finer stat groups.
**UI is the largest identified game-thread cost in the game and has never been profiled.**

### The remaining spike is the collapse itself

Worst 150 frames (64.8 ms) vs the rest (21.3 ms):

| | normal | spike | delta |
|---|---|---|---|
| AllWorkers/Physics | 1.74 | 28.39 | +26.65 |
| GameThread/EventWait/EndPhysics | 0.87 | 27.03 | +26.16 |
| GPU/ShadowDepths | 2.53 | 39.08 | +36.55 |
| TickActors | 1.71 | 12.66 | +10.95 |

Two drivers, both transient and both the same moment: pieces are simulating (with the game thread
stalled 27 ms waiting on physics) AND still casting shadows while they fall, because
`bDisableShadowsOnSettle` only fires once they settle.

**Next lever, NOT taken because it changes how the game looks:** drop shadow casting at RELEASE
rather than at settle. That is the 39 ms shadow spike, and falling rubble casting no shadow for ~10
seconds is a visual call for Michael, not a perf decision for an agent. The alternative is fewer
simulated bodies (`MaxSimulatedPieces`, currently 24 per collection).
