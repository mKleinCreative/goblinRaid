---
id: 309
title: World corruption 1/6 - the scalar, the sky, and a two-ended post-process grade from blown-out to gritty
agent: claude-corruption
status: review
claimed: 2026-08-25T22:59Z
build: none
waiting_on: NEVER COMPILED - build gate closed on 306 and 308. Needs an editor-closed build (3 new UCLASS types, so Live Coding cannot do it), then PIE in L_CombatArena: GS.Corruption.Set 1 and watch the sky and grade travel from blown-out to gritty. Both grade ends are EditAnywhere on the director for Michael to tune by eye.
evaluated: 2026-08-25T23:08:44Z
observed:
scenario:
files: 
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
  - Source/GoblinSiege/World/GSCorruptionDirector.h
  - Source/GoblinSiege/World/GSCorruptionDirector.cpp
  - Source/GoblinSiege/World/GSCorruptionDebugCommands.cpp
  - Config/DefaultGame.ini
---

## Goal

World corruption 1/6 - the scalar, the sky, and a two-ended post-process grade from blown-out to gritty

## Generate

Stage 1 of 6: the scalar and the output stage, driven by console. No drivers are wired.

**New - `Source/GoblinSiege/World/GSCorruptionSubsystem.h/.cpp`** (`UGSCorruptionSubsystem : UWorldSubsystem`, `Config = Game`).
Constant-rate follower (`FInterpConstantTo`, never `FInterpTo`), a high-water ratchet, a latching
console override, 0.25 stage bands with a `(New, Old)` delegate shaped to match
`AGSGameState::FGSOnAlarmPhaseChanged`, one timer at 10 Hz cleared in `Deinitialize`, static
null-safe `Get()`, and `DescribeState()`.

**New - `Source/GoblinSiege/World/GSCorruptionDirector.h/.cpp`** (`AGSCorruptionDirector : AActor`).
Found-or-spawned by the subsystem. Discovers SkyAtmosphere / ExponentialHeightFog / DirectionalLight
/ SkyLight, **captures their authored values as baselines**, and lerps from those - never absolutely.
Spawns a fog actor if the map has none, and its own unbound `APostProcessVolume` at priority 1000.

**The grade arc** (Michael, 2026-08-25 - *"the good world before the burning a little more bloom
heavy and almost blown out, then the darkness infested world a little more gritty"*). `FGSCorruptionGrade`
holds both ends as an `EditAnywhere` USTRUCT so the whole look retunes with no rebuild:

| Field | Clean | Gritty |
|---|---|---|
| BloomIntensity | 3.0 | 0.35 |
| BloomThreshold | -0.5 | 1.20 |
| AutoExposureBias | +0.75 | -0.40 |
| Saturation / Contrast / Gamma | 1.05 / 0.90 / 1.08 | 0.55 / 1.25 / 0.92 |
| Gain | neutral | 1.05, 0.92, 0.85 |
| FilmGrain / Vignette / SceneFringe | 0 / 0.15 / 0 | 0.60 / 0.60 / 0.80 |

**New - `Source/GoblinSiege/World/GSCorruptionDebugCommands.cpp`** (headerless):
`GS.Corruption.Set/.Release/.Step/.Dump/.Refresh`. Plus cvars `GS.Corruption.RiseRate`, `.FallRate`,
`.RatchetFraction`, all `ECVF_Cheat`.

**`Config/DefaultGame.ini`** - `[/Script/GoblinSiege.GSCorruptionSubsystem]` with commented entries.

**No `GoblinSiege.Build.cs` change**, as the plan predicted: every type used is in `Engine`.

## Evaluate

**NOTHING HAS BEEN COMPILED AND NOTHING HAS BEEN WATCHED.** The build gate is CLOSED on #306
(review, 16.1h) and #308 (review), so no build was possible. Everything below is reasoning about
source, and the one honest summary is: **this ticket has produced code that has never been through a
compiler.** It must not close as `done` on what follows.

**What I did verify, by reading the engine rather than trusting memory** - and three of the four
corrected something I had written or was about to:

- All ten `FPostProcessSettings` fields and their `bOverride_` flags exist in `Engine/Scene.h`.
  **The four grading fields are `FVector4`, not float** (`ColorSaturation:1514` ff) - XYZ is RGB, W
  is the master multiplier. Treating them as scalars compiles and writes garbage; the code puts the
  curve on W and the ash tint on Gain's RGB.
- **`SetMieScatteringColor` does not exist.** I had drafted against it. The real names are
  `SetMieScattering` / `SetMieAbsorptionScale`, plus `SetSkyLuminanceFactor`, which turned out to be
  the cleanest darkening knob available.
- `FogInscatteringLuminance` is the property behind `SetFogInscatteringColor` - the getter and setter
  are not the same word.
- `ALight::GetLightComponent() const` returns a non-const `ULightComponent*`, so the baseline capture
  works from a const actor.

**Structural check only:** braces and parens balance in all five files, and every member declared in
the subsystem header has a definition. That catches typos. It does not catch a wrong type, a missing
include, or a UHT rejection, and I should not be read as implying otherwise.

**Adversarially, what I expect to be wrong or to need watching:**

- **The clean end is a guess at Michael's words.** Bloom 3.0 with threshold -0.5 may read as "fogged
  windscreen" rather than "blown out pastoral". This is exactly the class of thing the project rule
  says to measure and hand over rather than iterate on, which is why both ends are EditAnywhere.
- **An always-live volume at priority 1000 takes over any level grade.** That reverses a promise the
  approved plan made. Accepted because the maps that matter have no PPV at all - but on
  `L_Hamlet_T1`, `L_Tutorial_Island` and `Village_Human` ours will sit on top of the artist's, and
  nobody has looked at those.
- **`RestoreBaselines()` is called from both `EndPlay` and `Deinitialize`** and is therefore
  double-called on a normal PIE stop. It is idempotent, so this is harmless - but it is belt-and-
  braces against the MPC-persistence class of bug, not a considered single owner, and if the order
  ever matters this is where it will bite.
- **`ComputeTarget01()` returns `CorruptionTarget01` when not overridden**, which is self-referential
  and does nothing in stage 1. That is correct-by-vacuity today and will be replaced wholesale in
  stage 2; it would be a real bug if anyone shipped stage 1 believing drivers existed.
- **`Set 0` then `Release` snaps the world back to the high-water mark.** That is ruling 41 working
  as designed, not a defect, but it will look like one the first time it happens.
- **The `bSpawnedVolumeOurselves` flag is written and never read.** Dead weight; kept only because
  the teardown path will want it when the volume stops being transient.

**Owes `AGENT_STATE.md`:** a BUILT line only once someone has watched it, and a DECISIONS line for
the always-live-volume reversal.

## Refine

**Changed in response to my own evaluation, during the pass:**

- Removed two placeholder lines (`UGSCorruptionSubsystemSettingsProxy`, `UGSCorruptionDirectorGateProxy`)
  that referenced types which do not exist. They were scaffolding I left behind while sketching, and
  they would have failed the build immediately. The two `Config` flags they stood in for are now read
  from the subsystem by the director, so a director spawned mid-session still honours them.
- Switched all five console delegates from `CreateStatic` to `CreateLambda`. `CreateStatic` wants a
  real function pointer; a captureless lambda usually converts, but "usually" is not a thing to spend
  a six-minute editor-closed build discovering.
- Added `Engine/Engine.h` for `GEngine`.
- Dropped `UPROPERTY(Transient)` from the director's five weak pointers to match the house rule
  `GSRaidDirector.h` states: nothing there is replicated, serialised or Blueprint-visible, and
  `TWeakObjectPtr` is safe without reflection.

**Deliberately left undone:**

- **The build.** Gate is closed; #306 and #308 must clear first.
- **Every driver** (stage 2/3), the data asset (4), the MPC (5), ash/embers/audio (6).
- **The audio layer's design has changed under us and I did not chase it.** #249 landed the mixer
  spine - `Content/Audio/Mix/` now has Classes, Submixes, Attenuation and Concurrency. Stage 6 should
  route through that, not the bare `UAudioComponent` the plan describes. Recorded, not built.

## Compile break fixed by claude-ui (2026-08-26) - #306 had right of way

This ticket had never compiled, and the first build that included it failed the WHOLE module:

    GSCorruptionSubsystem.cpp(10,1): error C2011:
        'FLogCategoryLogGSCorruption': 'struct' type redefinition

`DEFINE_LOG_CATEGORY_STATIC(LogGSCorruption, Log, All)` appeared in BOTH
`GSCorruptionSubsystem.cpp:10` and `GSCorruptionDirector.cpp:16`. Each is correct read on its
own; the unity build folds both into one translation unit and they collide. The other 18 errors
were all fallout - once the struct fails to define, every LOG macro against it fails too.

The project's own precedent is one DEFINE per category name: `LogGSRaid` lives only in
`GSRaidDirector.cpp`, and `GSRaidLibrary.cpp` takes a DIFFERENT name rather than redefining it.
Rather than rename and change which category the Director's lines appear under, the shared
intent was preserved: `DECLARE_LOG_CATEGORY_EXTERN` in `GSCorruptionSubsystem.h` (which the
Director already includes), `DEFINE_LOG_CATEGORY` once in the subsystem, duplicate deleted.

Files touched here belong to this ticket, not to #306. Done because the break was total and
#306 holds the lower number; flagged to Michael at the time rather than quietly.

