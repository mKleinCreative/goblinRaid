---
id: 405
title: General lag in the middle of the village during a raid
agent: claude-villagelag
status: review
claimed: 2026-09-09T19:31Z
build: none
waiting_on:
evaluated: 2026-09-09T19:50:26Z
observed:
scenario:
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
