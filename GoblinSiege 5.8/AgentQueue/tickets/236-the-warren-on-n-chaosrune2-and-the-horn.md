---
id: 236
title: The Warren on N_ChaosRune2, and the horn that fills it: arrival mouth, respawn, loot bank, tap-or-hold summon to a squad of 10
agent: claude-warren
status: done
claimed: 2026-08-21T17:51Z
build: required
waiting_on:
evaluated: 2026-08-21T22:08:05Z
observed: 2026-08-21T22:22:52Z | Michael watched a carried pig vanish into the Warren for 40 loot, watched loot DROPPED in the mouth get swallowed, and ran GS.Warren.Status which reported: Warren at (-2300,0,-70) arrival:Y respawn:Y banks:Y - 40 loot from 1 item(s), and score: loot=40 (source seen: yes) deeds=0. That score line is the proof AddLoot now reaches UGSScoreSubsystem - the GDD had it recorded as a live defect with zero callers. Horn watched summoning: 1 answered on a tap, 9 answered streaming out one at a time on a long hold. He also died and respawned, but WHERE is unverified - RespawnPlayer and ChoosePlayerStart log nothing and no player death or respawn line exists in the log.
scenario: PIE in L_CombatArena, Warren_Arena_01 at (-2300,0,-70), player pawn carrying and dropping BP_Livestock_Pig_TEST and the loot containers; roughly ten PIE sessions between 22:13 and 22:22 UTC.
files: 
  - Source/GoblinSiege/Raid/GSWarren.h
  - Source/GoblinSiege/Raid/GSWarren.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Core/GSGameMode.h
  - Source/GoblinSiege/Core/GSGameMode.cpp
  - Source/GoblinSiege/Interaction/GSInteractableComponent.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Raid/GSRaidDebugCommands.cpp
  - docs/goblin-siege-gdd.md
  - docs/decisions-ledger.md
---

## Goal

The Warren on N_ChaosRune2, and the horn that fills it: arrival mouth, respawn, loot bank, tap-or-hold summon to a squad of 10

## Generate

**`Raid/GSWarren.{h,cpp}` (new, `AGSWarren : AActor`)** — deliberately AGSRunicSite's twin, so whoever
fixes a spawn bug in one goes looking in the other. Root + `BankingSphere` (500uu, QueryOnly,
overlapping Pawn *and* WorldDynamic/PhysicsBody so it notices a dropped sack, which the runic site
never had to) + empty `MouthMesh` + `WarrenFX` with `bAutoActivate` **on** (unlike the portal, which
must stay dark until it opens; a Warren nobody can see is a Warren nobody can find). Three lookups
(`FindNearest`, `FindNearestArrivalMouth`, `FindNearestRespawnPoint`) share one templated
`FindNearestMatching` that filters *before* distance, so a banking-only Warren cannot win an arrival
query by being closer. `GetArrivalTransform` / `GetSpawnTransform` both route through the existing
`UGSRaidLibrary::FindStandableSpotNear` — no third copy of that trace, and deliberately not navmesh
projection. Banking: `BankCarriedLoot` (pawn walks in carrying), `BankLooseActor` (a courier's
`PutDown`, or anything thrown in), both funnelling into `CommitBank`. A `BeginPlay` sweep banks
anything already lying in the mouth at level start.

**`Interaction/GSInteractableComponent.h`** — `int32 LootValue = 0` + `GetLootValue()`. The seam the
project never had: nothing could answer "what is this worth". On the existing component rather than a
new `UGSLootValueComponent`, because every bankable actor already owns one and a new `UCLASS` costs a
six-minute editor-closed build here. Nothing in the interaction framework branches on it.

**`Horde/GSHordeSubsystem.{h,cpp}`** — three changes:

1. `FindArrivalTransform` prefers the nearest arrival-mouth Warren; the `Marker.HordeArrival` path is
   untouched underneath it as the fallback every pre-existing map still uses.
2. `ResolveDeliveryLocation` puts the Warren **above** the runic site, so couriers turn in at the hole.
3. **`SummonWave` split into `SummonOne`** (+ `GetSummonableNow()`, + a never-reset `SpawnOrdinal`
   that rotates a 5-wide lateral offset so goblins climbing out on the same tick do not share a
   capsule). The single-debit rule of decision 40 moves intact from `SummonWave` to `SummonOne`;
   `SummonWave` is now a loop over it, kept for `GS.Horde.SpawnTest`.

**`Weapons/Abilities/GSGA_Horn.{h,cpp}`** — tap-or-hold. Activation raises `bHornHeld` and keeps the
existing wind-up (`BlastDurationSeconds`), after which `SummonNext` delivers one goblin and re-arms
itself every `SummonIntervalSeconds` (0.55) while held. Four distinct stop reasons, logged by name:
released / nothing answered / per-hold ceiling / squad full or pool dry. `MaxSummonsPerHold = 0` means
"until the active cap" — the squad of 10. `EndAbility` overridden to clear the timer and reset blast
state, which `InstancedPerActor` makes mandatory rather than tidy.

**`Characters/GSPlayerCharacter.{h,cpp}`** — `Input_HornReleased` bound to `HornAction` **Completed**,
reaching the live ability via `FindAbilitySpecFromClass(...)->GetPrimaryInstance()`.

**`Core/GSGameMode.{h,cpp}`** — a private `bRespawning`, raised by a `TGuardValue` around
`RestartPlayer` inside `RespawnPlayer`. `ChoosePlayerStart` prefers a Warren **only** while that flag
is up; `RestartPlayerAtPlayerStart` gains an `AGSWarren` branch beside the existing `AGSRunicSite` one.

**`Raid/GSRaidDebugCommands.cpp`** — `GS.Warren.Status`: every Warren and what it is serving, the
score subsystem's loot/deeds, and the horde's reserve/active/summonable. Prints an explicit line when
the level has **no** Warren, which is the most likely reason the feature looks broken.

**Docs** — GDD §6 (Warren rewritten to BUILT, placed-not-planted), §9 ("once placed"), §5 (new
tap-or-hold paragraph), §12.1 row 13 re-graded; `Tools/check_gdd.py` **CLEAN, 10 checks passed**.
Ledger entries **36-39**.

## Evaluate

**NOTHING HERE HAS BEEN COMPILED OR RUN.** The build gate was closed for this ticket's whole life
(#235, claude-acf, ACF Phase 2b-2b) and the editor was open throughout, which `Build-GoblinSiege.ps1`
refuses. Every claim below is a claim about source, and the project's own rule is that a class which
compiles is not a system that runs — this one has not even compiled.

**Verified by evidence:** only the GDD parser contract (`check_gdd.py` CLEAN), and that the APIs this
leans on still exist post-ACF-Phase-2 (`AddLoot`, `GetDeeds`, `DestroyCarried`, `IsCarrying`,
`GetCarriedActor`, `FindStandableSpotNear` all re-grepped after restoring the parked files).

**Written and never run — the whole feature.** Specifically at risk:

- The overlap channel set on `BankingSphere` is reasoned, not measured. If `BP_Livestock_Pig`'s root
  sits on a channel I did not name, a dropped pig will lie in the mouth and never bank. First thing to
  check if banking looks dead.
- `Spec->GetPrimaryInstance()` returning null for an `InstancedPerActor` ability that has been granted
  but never activated is a real possibility. The release path no-ops safely if so — which would mean a
  held horn never stops. **This is the single most likely runtime failure.**
- `bRespawning` + `ChoosePlayerStart`: `L_CombatArena` has no `AGSRunicSite`, so the "first spawn still
  uses the runic site" half of the design cannot be proven there at all.

**Touched outside the goal:** nothing. The changed-file set is exactly the claim.

**AGENT_STATE.md owes a DECISION line** for rulings 36-39, and a line recording that
`NotifyGoblinSpentOnWarren` is now deliberately caller-less, so a later bloat audit does not delete it.

**Content half not started, and it is required before any of this can be watched:** `BP_GS_Warren`
(parented to `AGSWarren`, `WarrenFX` = `N_ChaosRune2`), a placed instance in `L_CombatArena`, and
`LootValue` set on `BP_Livestock_Pig` (40) and the loot containers (15). The VibeUE MCP tools are not
loaded in this session, so no editor work was possible.

## Refine

**Done after the MCP editor connection came back (2026-08-21, mid-ticket):**

6. **`AM_GS_HornBlast` created** — `/Game/Characters/ScoutV2/Montages/AM_GS_HornBlast`, wrapping
   `A_MX_Taunt_Battlecry_Gob` (GOB_Scout_v2_Skeleton, 2.833s, DefaultSlot, blend 0.25/0.25 to match
   `AM_GS_ThrowTorch`). Verified by re-reading the saved asset, not by trusting the create call.
   **This is a placeholder and the ticket should not pretend otherwise:** there is no horn animation
   anywhere in the project, and a battlecry with no horn in the hand is a goblin shouting.
7. **`HornMontage` now defaults to it in the `UGSGA_Horn` constructor.** Not a Blueprint default,
   because the horn is the only ability with no Blueprint subclass and `HornAbilityClass` is
   hard-defaulted to the C++ class — there is nowhere for a designer to set it. Left
   `EditDefaultsOnly` and soft, so a `GA_GS_Horn` overrides it the day one exists.

   **UNCLAIMED FILE, declared rather than hidden:** `Content/Characters/ScoutV2/Montages/AM_GS_HornBlast.uasset`
   is a new asset that was not in this ticket's claim. Nothing else could have held it (it did not
   exist), but the rule is claim-before-write and I wrote it without one.

   **Feel question for whoever watches this:** the montage is 2.833s and the blast wind-up is 1.6s,
   so on a long hold the goblin finishes the animation while goblins keep climbing out. Looping the
   montage while held is the obvious fix and was deliberately not done before anyone has seen it.

**Changed in response to my own evaluation, before handing back:**

1. **`CommitBank` now broadcasts BEFORE destroying the cargo.** The first pass destroyed it and then
   broadcast a pending-kill pointer into Blueprint. Every listener this hook exists for — a burst at
   the mouth, a bark naming what went in — wants the cargo's location or class, and handing a designer
   a dead actor to read those off is a crash waiting for the first missing `IsValid`.
2. **`BankLooseActor` refuses anything with an attach parent.** Without it, a courier walking its cargo
   in would have that cargo banked out from under it by the loose path a frame before its own
   `PutDown`, leaving the carrier's attach and move-speed effect dangling.
3. **`BankCarriedLoot` uses `PutDown()` rather than `DestroyCarried()`**, so the destroy happens inside
   `CommitBank` after the broadcast — same unwind, correct ordering.
4. **Added `Engine/Engine.h`** to `GSWarren.cpp`; `FindNearestMatching` uses `GEngine` and was relying
   on a transitive include.
5. **Corrected the `OnLootBanked` header doc**, which still said the source was already destroyed.

**Deliberately left undone:**

- **The horn PROP.** `SM_HuntingHorn_Signal01` exists and is still orphaned; nothing attaches it. See
  the next bullet for why that half is deliberately held.
- **The prop-attach half**, which wants a `HornMesh`/`HornSocket`/`HornMeshOffset` trio on
  `UGSWeaponDataAsset` copying the existing `HeldTorchMesh` pattern. Held back on purpose: ACF's
  Inventory System owns equipment slots and attach sockets, and adding fields to our own weapon data
  asset days before that migration reaches equipment is how work gets done twice.
- **`AddDeeds` at the Warren**, permanently. See ruling 39 and the warning in `GSWarren.h`.

> 2026-08-21T18:42Z Session recovered after an accidental close (transcript acbf9b15). Michael has now WATCHED tap-and-hold summon work, which answers the ticket feel question: loop the montage while held. Scope extended by his ask - loop the blast, make the pose read as blowing a horn, and give the horn a sound. Three synthesized horn candidates delivered for him to pick from; nothing in the 2000-asset sound bundle is a horn.


---

## Refine, second pass (session recovered 2026-08-21)

The original session was closed accidentally mid-diagnosis; transcript `acbf9b15` is intact and this
is the same body of work continued. Michael has since **WATCHED tap-and-hold summon a squad**, which
retires the ticket's open feel question with a ruling: loop the blast while held.

### Done, source only - NOT COMPILED, NOT RUN

The build gate has been closed for this ticket's entire life (#235, #237, #238 still open) and the
editor is shut, so this is again a claim about source and nothing more.

**`GSGA_Horn.{h,cpp}` - the blast now lasts as long as the button.**

1. **The montage loops.** `StartBlast` points the montage's first section at itself via
   `Montage_SetNextSection`, which is *exactly* the trick `UGSGA_Block` already uses to hold a 1.83s
   guard pose for an arbitrarily long hold - same idiom, same reasons, and the precedent's own
   comment explains them: montage sections are not reachable from the Python asset API, and a
   montage marked looping in the asset would loop everywhere it is ever used. Deliberately NOT a new
   asset-side loop.
2. **`StopBlast` unhooks the chaining before stopping.** The section link lives on the *anim
   instance*, not the montage, and outlives the ability. Left hooked, the next thing to play
   `AM_GS_HornBlast` on that goblin would loop forever with nobody holding a button. This is the
   single subtlest thing in the change and the one most likely to be "simplified" away later.
3. **The horn has a voice.** `HornSound`, re-struck once per montage cycle on `SoundCycleTimerHandle`
   so audio and animation stay locked however long the hold runs. Re-articulating on the loop point
   rather than sustaining one endless note is deliberate: a held horn should read as a goblin
   blowing *again*, not as a stuck sound. Previous note is faded before the next is struck so long
   samples cannot stack.

**This is the first sound anywhere in `Source/GoblinSiege`** - a grep for `USoundBase`,
`UAudioComponent`, `SpawnSoundAttached` and `PlaySoundAtLocation` across the whole module returns
nothing. So it spawns a bare attached component and reaches for no SoundClass, submix or attenuation
asset the project does not own. One call site to revisit when audio gets a spine, not a system.

**Verified by evidence:** only the API contracts, re-read out of the 5.8 engine headers rather than
recalled - `SpawnSoundAttached` overload at `GameplayStatics.h:780`, `Montage_SetNextSection`
at `AnimInstance.h:679`, `Montage_Stop` at 639, `UAudioComponent::FadeOut` at 511. Everything else is
unrun.

**Known feel item for whoever watches it:** release does not stop the horn instantly. `SummonNext`
owns the end, so the loop runs on for up to `SummonIntervalSeconds` (0.55) past the button coming up.
That reads as finishing the note and is probably right, but nobody has seen it.

### The sound: created, because it could not be found

Swept all ~2000 sound assets in `NaPH_RPG_Fantasy_Sounds_Bunle` plus every plugin. It has animals,
crafting, doors, dungeon, environment, explosions, footsteps, human vocalisations, interface, magic,
misc, monsters and weapons - **and no brass of any kind**. `SM_HuntingHorn_Signal01` is a mesh.

Three candidates synthesized (`scratchpad/horn/synth_horn.py`, additive brass model where harmonic
content tracks blowing pressure, which is the part that stops it sounding like a sawtooth with an
envelope on it) and sent to Michael to choose by ear:

| file | f0 | length | character |
|------|----|--------|-----------|
| `SW_GS_Horn_A_DeepWar` | 116 Hz (Bb2) | 2.85s | deep lur, mournful, carries |
| `SW_GS_Horn_B_Signal`  | 175 Hz (F3)  | 2.35s | brighter, cuts through combat |
| `SW_GS_Horn_C_GoblinGruff` | 98 Hz (G2) | 2.80s | 27 Hz growl, heavy breath, badly-made |

`HornSound` is hard-defaulted to `/Game/Audio/Horn/SW_GS_HornBlast` - **an asset that does not exist
yet**. The soft pointer resolves to null and the ability logs nothing and summons fine, so this is
inert until the chosen wav is imported to that exact path.

### BLOCKED, and why

- **Import the chosen wav** to `/Game/Audio/Horn/SW_GS_HornBlast`. Needs the editor open.
- **Compile.** Needs the build gate, which #235/#237/#238 hold closed.
- **"Make it look like he's blowing the horn."** Needs the editor AND a ruling - see below.

### The pose is not a pose problem

Re-swept the goblin's whole animation library - 168 clips across DTA combat, DK2 locomotion, the
climb/traversal sets and the Mixamo set. **There is no hand-to-mouth clip of any kind**: no drink, no
eat, no horn, no trumpet. `A_MX_Taunt_Battlecry_Gob` remains the only thing whose silhouette is even
adjacent (head back, one arm raised).

The trap here is treating this as an animation task. **The hand is empty.** `SM_HuntingHorn_Signal01`
exists and is still orphaned - nothing attaches it - and no arrangement of an empty hand near a mouth
reads as blowing a horn. It reads as a goblin shouting, which is what it reads as today. The prop is
the larger half of Michael's ask and it is the half this ticket **deliberately deferred**, because
ACF's Inventory System owns equipment slots and attach sockets and adding `HornMesh`/`HornSocket` to
our own `UGSWeaponDataAsset` days before that migration reaches equipment is how work gets done twice.

That deferral was correct when the ask was "add a horn animation". It is now blocking a thing Michael
has explicitly asked for, so it is his call and not mine. Put to him rather than guessed at.

### Two rulings from Michael that changed the design mid-pass

1. **"A continuous note for as long as they hold it down."** This retired the re-struck-per-montage-
   cycle design described above before anyone heard it. The voice is now three parts - attack,
   seamless sustain loop, release - and the audio is deliberately NO LONGER tied to the montage
   length. `RestrikeHornSound` and `SoundCycleTimerHandle` are gone; `StartHornVoice` /
   `BeginHornSustain` / `StopHornVoice` and `VoiceHandoffTimerHandle` replace them. The handover
   from attack to sustain is timed off `USoundBase::GetDuration()` of the attack asset rather than a
   constant, so re-authoring the attack cannot open a gap at the join.

2. **"The horn should start low and go up a few octaves."** The rise lives in the ATTACK, because a
   pitch that rises forever cannot also loop. The attack climbs two octaves and lands exactly on the
   sustain's pitch; the loop holds at the top. The climb is an overblow through the harmonic series
   rather than a siren glide - a real horn's lip jumps between partials, and `catch` in the synth
   table is the dial for how hard it snaps to them.

**Seamlessness is measured, not asserted.** A loop that clicks once per cycle is worse than no loop.
Three things had to be true and each was verified: integer periods AND integer samples in the loop
(f0 is solved for, not chosen); vibrato closing its own whole number of cycles; and the IIR filter
state converged by filtering two copies of the loop and keeping the second - that last one alone
took the wrap discontinuity on the deep horn from 3x a normal sample step to 0.03x. Final joins,
as a ratio against the waveform's own largest sample-to-sample step (1.0 = indistinguishable):

| variant | climb | wrap | start->loop | loop->end |
|---------|-------|------|------|------|
| A DeepWar | 117 -> 467 Hz / 1.45s | 0.03 | 0.09 | 0.12 |
| B Signal | 130 -> 520 Hz / 0.85s | 0.08 | 0.06 | 0.06 |
| C GoblinGruff | 98 -> 392 Hz / 1.20s | 0.08 | 0.37 | 0.00 |

Pitch tracked out of the rendered attacks to confirm the climb is real and stepped, not asserted:
A runs 116 -> 230 -> 350 -> 466 Hz, which is partials 1, 2, 3, 4.

**Michael's ruling on the prop: attach it our way now.** `HornMesh`/`HornSocket`/`HornMeshOffset` on
`UGSWeaponDataAsset` copying the existing `HeldTorchMesh` pattern, plus a baked `A_GS_HornBlow_Gob`
with the right arm rewritten to hold the horn at the mouth. This knowingly overrides the earlier
deferral to ACF's Inventory System and will be redone when that migration reaches equipment; it is
his call and it is recorded here so the rework is expected rather than discovered.

**STILL BLOCKED - nothing below has been done:**
- Import the chosen wav set to `/Game/Audio/Horn/SW_GS_HornBlast_{Start,Loop,End}`, with **Looping
  ticked on the Loop asset** - nothing in C++ re-triggers it, so that flag is the whole sustain.
- Compile. The gate is held closed by #235, #237 and #238.
- The prop attach and the baked clip. Both need the editor open.

### Third pass: it is a PVC pipe horn, not a brass one

Michael linked a video. **I cannot watch video and did not** - the only thing extracted was the
title, "PVC Goblin Battle Horn", via a page fetch. That title alone was worth more than the clip
would have been, because it names the instrument, and the instrument was wrong in every version
above.

A brass lur has a CONICAL bore: resonances at 1:2:3:4, overblows at the octave, rings. A length of
PVC pipe is a CYLINDER: resonances at **1:3:5 - odd multiples only** - and it overblows at the
**twelfth**, not the octave. Plastic also damps where brass rings, and a homemade horn buzzes.
So the previous three candidates were a well-built instrument that is not the one in the reference.

`synth_pvc_horn.py` models the cylinder. `even_atten` suppresses the even partials, `ladder` is the
pipe's actual reachable notes, and the climb snaps to it. **Verified by measurement, not by
assertion:**

| variant | pipe | climb (measured) | odd-vs-even |
|---------|------|------------------|-------------|
| P1 PipeDeep | 87 Hz, 1-3-5 | 87 -> 261 -> 425 Hz (1x, 3x, 4.89x) | odd lead 15.4 dB |
| P2 PipeTwelfth | 110 Hz, 1-3 | 110 -> 323 Hz (1.00x -> 2.94x) | odd lead 16.0 dB |
| P3 PipeRagged | 73 Hz, 1-3-5 | 73 -> 350 Hz (1.14x -> 4.79x) | odd lead 13.4 dB |

The odd-vs-even figure is the one that says this is a pipe and not a horn: 13-16 dB of separation
between the odd and even partials in the sustain. P3 spends 22 of 30 frames *between* rungs rather
than on them - that is `catch=0.55` and it is deliberate, a sloppy player sliding instead of
catching the partials. P1 and P2 sit on the rungs.

**Measurement note, recorded because it nearly produced a false bug report:** the first two attempts
to verify the climb both used the tracker's own first frame as the baseline, and that frame is
breath, not a note. Against a 105 Hz "baseline" P1 appeared to climb to 4.0x and land off-ladder.
Measured against the pipe's known 87 Hz fundamental it lands on 4.89x, which is 5x. The synthesis
was correct both times; the instrument was the thing being measured wrongly.

**Not done and not attempted: extracting the audio from the video.** Shipping it would be shipping
unlicensed audio regardless of whether the tooling existed.

### Fourth pass: the synthesis is retired, the horn is a real recording

Michael licensed and downloaded Boom Library's free Nordic War Horns pack and chose "To Valhalla".
The synthesised candidates above are superseded and kept only as a record of how the requirement was
arrived at. `cut_valhalla.py` + `wav24.py` (scratchpad) cut the source into the three assets.

Source is 96 kHz 24-bit stereo BWF, 45.4s, six takes. **Take 2 (15.90-24.46s)** is the one used: it
carries 5.94s of pitch-steady 78 Hz, the longest stable sustain in the file and the only one long
enough to find a good loop inside. Output is 44.1 kHz 16-bit mono, DC-corrected, peak -1 dB, the
three parts normalised together so their relative levels survive.

- `SW_GS_HornBlast_Start` 0.745s (15.855 -> 16.60s)
- `SW_GS_HornBlast_Loop`  1.400s (16.60 -> 18.00s, chosen by correlation, r = 0.974)
- `SW_GS_HornBlast_End`   2.800s (from 20.174s, r = 0.973, gain-matched +2.5 dB to the loop)

Nothing is pitch-shifted or time-stretched. The horn is left as recorded.

**A wrong "fix", recorded because the number that exposed it is the useful part.** The loop->end
join measured 1.33 sample-steps, so a 12ms fade-in was added to the release - and the seam went to
**17.95**, thirteen times worse. Starting the release at zero against a loop ending at full
amplitude is a dropout, not a smoothing. The actual error was the measurement: `StopHornVoice`
fades the loop out over 60ms and spawns the release in the same call, so the ENGINE crossfades them
and the two are never butted together. The preview now models that crossfade instead of splicing,
and the fade-in is gone. Final joins, as the game plays them: wrap 0.12, start->loop 0.12,
loop->end 0.14.

**To Valhalla does not rise, and Michael asked for a rise.** Pitch-tracked across all 49 files in
the pack: To Valhalla trends -0.03 octaves - it is a flat, deep, steady note. The only single-gesture
riser in the pack is **`HORNMisc_WAR HORN-Odins Organ`, which trends +1.18 octaves** (46-244 Hz,
2.41 octave span). `BIRCH BARK LUR-Multiple Melodies` trends +2.10 but is several distinct notes
rather than one climbing blast. Put to Michael rather than substituted - he named To Valhalla.

**Michael's ruling, 2026-08-21: To Valhalla is the horn.** The open question above - that it trends
-0.03 octaves and so does not deliver the rise he asked for earlier - is answered by choosing it
anyway. Odins Organ (+1.18 oct) is NOT to be substituted. The three assets already cut from take 2
are final; the rise requirement is retired, not deferred.

> 2026-08-21T21:37Z Source + content complete for the horn: sound imported, prop socket wired, clip baked, three montages. Awaiting build then observation.

### The two things that made this unobservable are now done

**A Warren is placed.** `Warren_Arena_01` in `L_CombatArena` at (-2300, 0, -70), ground-traced onto
`Arena_Floor` rather than guessed - the floor surface is at z -70, not the z 130 the PlayerStart and
the HordeArrival markers sit at. `isinstance(w, unreal.GSWarren)` confirms the Blueprint really is an
`AGSWarren`, and both role flags default true: **arrival mouth AND respawn point**. Level saved, no
dirty map packages.

This is a stopgap. #245 replaces it with the player planting one on X, which is Michael's ruling of
2026-08-21 and reverses ruling 36. The placed instance should be removed when #245 lands.

**`LootValue` is finally set** - the thing the crashed session failed three separate ways to write:

| asset | LootValue | GDD 10 |
|-------|-----------|--------|
| `BP_Livestock_Pig` | 40 | livestock 40/25/10 |
| `BP_LootBarrel` | 15 | loot 5-25 |
| `BP_LootCrate` | 15 | loot 5-25 |
| `BP_LootChest` | 25 | loot 5-25 |

**What made it work, recorded because three approaches did not:** the write has to go to the
component TEMPLATE, and the only accessor that returns templates is
`SubobjectDataBlueprintFunctionLibrary.get_object` - which is **deprecated**, and whose deprecation
message says the replacement `GetAssociatedObject` *"will not return template objects"*. The modern
API is the wrong tool here. Then `BlueprintEditorLibrary.compile_blueprint` before `save_asset`, or
the value does not survive. Verified by re-reading after compile and save, and again on the four
placed instances, which inherit correctly: **95 points of bankable loot on the arena floor**,
309-570 uu from the Warren.

`AGSWarren::DescribeStatus` is a plain C++ method, not a `UFUNCTION`, so it is not callable from
Python. `GS.Warren.Status` is the way to read it.

> 2026-08-21T22:08Z Warren placed in L_CombatArena and LootValue set on all four interactables. Both observation blockers cleared - ready to watch.
