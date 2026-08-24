---
id: 249
title: Audio phase A: mixer spine - sound classes, submixes, attenuation, concurrency, surface types
agent: claude-audio
status: abandoned
claimed: 2026-08-21T22:55Z
build: none
waiting_on:
evaluated: 2026-08-21T23:06:44Z
observed:
scenario:
files: 
  - Config/DefaultEngine.ini
  - Content/Audio
  - .gitignore
---

## Goal

Audio phase A: mixer spine - sound classes, submixes, attenuation, concurrency, surface types

## Generate

The project had **no audio architecture of any kind** — zero SoundClass, SoundMix, SoundSubmix,
SoundAttenuation, SoundConcurrency and PhysicalMaterial assets, and no `PhysicalSurface` names. Every
one of the ~1,050 pack cues played unrouted, unattenuated and voice-unlimited. This ticket is the
spine, and it is **content + ini only — no `.cpp`, no build**.

**47 assets created under `Content/Audio/`** (all via `execute_python_code`, idempotent
`does_asset_exist` guard before every create, every op logged CREATED/SKIPPED/MODIFIED):

- **13 SoundClasses** (`Mix/Classes/SC_GS_*`): Master → Music / Ambience / UI / Voice{Human,Creature}
  / SFX{Player,NPC,Impact,World,Signal}. `SC_GS_Music` is_music + vol 0.80; `SC_GS_UI` is_ui_sound,
  apply_effects=false, apply_ambient_volumes=false (so the pause menu is not silent and UI takes no
  reverb); `SC_GS_SFX_Signal` always_play (the horn is never voice-stolen).
- **9 SoundAttenuations** (`Mix/Attenuation/ATT_GS_*`): Footstep 150/1600, Foley 100/700,
  Impact 250/3500 (+occlusion/LPF), WeaponSwing 150/2000, Voice_NPC 400/5000 (+listener focus),
  **Signal_Long 2500/14000 logarithmic, occlusion off** so the horn crosses the hamlet,
  Fire_Loop 300/2800 (non-spatialized radius), Structure 500/6000, Ambience_Bed 2000/8000.
- **10 SoundConcurrencies** (`Mix/Concurrency/CC_GS_*`): Footsteps 10, Impacts 12, Voice_Global 5,
  **Voice_PerActor 1 with limit_to_owner=true** (one actor, one voice — the row that stops eight
  guards screaming at once), Fire 8, Structure 6, UI 4, Signal 1, Ambience 1, Default 32.
- **7 SoundSubmixes** + **`SFX_GS_MusicDuck`**, a `SubmixEffectDynamicsProcessorPreset` on
  `SM_GS_Music`'s effect chain: COMPRESSOR, key_source=SUBMIX, external_submix=`SM_GS_Voice`,
  4:1, −22 dB, attack 10 ms, release 250 ms. This is the only submix effect in the project, on
  purpose — each one costs CPU whether or not anything is playing.
- **`SMix_GS_Base`** SoundMix — the vehicle for runtime class overrides (options sliders, and the
  music proximity swell in phase F).
- **7 PhysicalMaterials** (`Content/Audio/Surfaces/PM_GS_{Dirt,Grass,Stone,Wood,Thatch,Water,Metal}`) —
  **created, deliberately not painted onto anything** (see Refine).
- **Routing:** `default_submix` set on all 12 non-master classes.

**`Config/DefaultEngine.ini`** (backed up outside the repo before editing; CRLF and ASCII-only
preserved, verified with `file`):
- `AudioMaxChannels` **0 → 64**. Zero means *unlimited voices*, which in a 10-goblin-horde game with
  an 8-neighbour fire spread is a mix that degrades without ever telling you.
- New `[/Script/Engine.PhysicsSettings]` with SurfaceType1..7 = Dirt/Grass/Stone/Wood/Thatch/Water/
  Metal, under a comment block stating the numbering is **permanent and append-only** (materials
  store the index, not the name).
- New `[/Script/Engine.AudioSettings]` registering DefaultSoundClassName, DefaultMediaSoundClassName,
  DefaultSoundConcurrencyName, DefaultBaseSoundMix, MasterSubmix, ReverbSubmix.

**Horn wired through the spine with no code change.** `GSGA_Horn` spawns its three SoundWaves bare,
so setting class/attenuation/concurrency on `SW_GS_HornBlast_{Start,Loop,End}` routes it:
`SC_GS_SFX_Signal` + `ATT_GS_Signal_Long` + `CC_GS_Signal`. `GSGA_Horn.cpp` was **not touched** —
its migration to tag-posting is phase C.

## Evaluate

**Nobody has heard a single thing yet, and the spine is not live.** That is the headline. The
`[/Script/Engine.AudioSettings]` block — master submix, default sound class, default concurrency,
base mix — is read at **editor startup**, and the editor has not been restarted. Until it is, the
assets exist and are correctly configured but the audio device is still running on engine defaults.
Everything below is structural evidence, not behavioural evidence.

**What IS verified, by reading back from disk after saving (not by the create call returning true):**
- The SoundClass tree was walked recursively from `SC_GS_Master` and printed with each node's parent,
  volume and flags — all 13 present, parented correctly, flags as intended.
- All 9 attenuations re-read: inner radius, falloff, distance model and occlusion match the spec.
- All 10 concurrencies re-read: max count, resolution rule, and `limit_to_owner=True` confirmed on
  `CC_GS_Voice_PerActor` specifically.
- `SM_GS_Music.submix_effect_chain` re-read → `['SFX_GS_MusicDuck']`, and the preset re-read →
  COMPRESSOR / SUBMIX / external=SM_GS_Voice / 4.0 / −22.0 / 250 ms.
- All 12 class→submix routes re-read and printed.
- The three horn waves re-read showing class, attenuation and concurrency.
- The ini re-read by grep; `git diff --stat` = 35 insertions, 1 deletion, one file.

**What is written and has never run:** all of it, audibly. Specifically unproven: that music actually
ducks under the horn; that `ATT_GS_Signal_Long`'s 14,000 uu falloff is the right size for a hamlet
nobody has built yet; that 64 voices is enough; that −22 dB / 4:1 is a duck you want rather than a
pump. **Every number in this ticket is a starting point chosen from reasoning, not from listening.**

**Three things did not go as planned, and the workarounds are compromises worth naming:**

1. **Submix parenting is implicit, not explicit.** Both `parent_submix` and `child_submixes` are
   **read-only from Python** in 5.8 — I tried both directions and got "is read-only and cannot be
   set". The topology is instead achieved by `MasterSubmix=SM_GS_Master` in the ini: submixes with no
   parent attach to the master automatically. The result should be the intended tree, but the Submix
   editor graph will *look* flat, and **that reasoning is untested until restart.** If it turns out
   orphan submixes do not attach as expected, the fallback is dragging six nodes in the Submix editor
   by hand, once.
2. **`SM_GS_Voice` is doing double duty as the duck-key bus.** A dynamics processor keys off exactly
   one external submix, and the design wants music to duck under **both** voice and the horn. So
   `SC_GS_SFX_Signal` routes to `SM_GS_Voice` rather than `SM_GS_SFX`. It is functionally right and
   the name is now slightly wrong — a designer opening `SM_GS_Voice` will find the horn in it. The
   honest alternative was a seventh `SM_GS_Duck` submix; I judged one misleading name cheaper than an
   extra asset, but I would not defend that hard.
3. **`surface_type` is unset on all 7 physical materials.** `unreal.PhysicalSurface` only exposes
   `SURFACE_TYPE_DEFAULT` until the ini names are loaded, and the int cast is rejected. So the
   assignment is a **post-restart follow-up**, and the materials are inert until then. Nothing depends
   on them yet, so this blocks nothing before phase D.

**Touched outside the goal:** the three horn SoundWaves under `Content/Audio/Horn/`, which are not in
my claimed file list. They are not claimed by any open ticket (checked), and the change is three
property assignments with no code impact — but it is scope I took without claiming, and it should be
in the record rather than discovered later.

**Owed to AGENT_STATE.md — a DECISION line:** *the project now has a mixer spine under
`Content/Audio/Mix`, and the SurfaceType1..7 numbering in DefaultEngine.ini is permanent
and append-only.*

**Gotcha worth adding to CLAUDE.md §"Editor Python gotchas"** (not added — CLAUDE.md is not mine to
edit unclaimed, flagging for Michael): **`EditorAssetSubsystem.load_asset()` intermittently returns
`None` for assets that demonstrably exist**, right after those assets have been created and saved in
the same session. `does_asset_exist()` returned True and `unreal.load_asset()` / `unreal.load_object()`
returned the object for all seven submixes in the same script where `EAS.load_asset()` returned None
for all seven. CLAUDE.md currently warns that `EditorAssetLibrary` lies and says to use
`EditorAssetSubsystem` instead — **the subsystem lies too, at least for freshly-created assets.**
Use the global `unreal.load_asset()`. This cost two failed script runs.

## Refine

**Changed in response to the above:**
- Switched every load to the global `unreal.load_asset()` after the subsystem started returning None
  mid-script, and re-verified all affected assets by reading them back from disk rather than trusting
  the earlier writes.
- Added the permanence warning comment block above the surface list in the ini after realising a
  future agent appending a surface in the "wrong" alphabetical place would silently repaint every
  material in the project.
- Set `always_play` on `SC_GS_SFX_Signal` — not in the original plan. The horn is the one sound whose
  absence is a gameplay failure, not a mix failure, and concurrency alone does not protect it from
  the 64-voice cap.
- Added `DefaultMediaSoundClassName` alongside `DefaultSoundClassName` so media playback does not
  bypass the spine.

**Deliberately left undone:**
- **No physical-material painting pass.** Michael's ruling was "footsteps are fine for now", and
  `L_Groatsworth` does not exist yet — painting surfaces onto a map that has not been built is the
  premature half. Everything resolves to Default → Dirt fallback until phase D.
- **No `SC_GS_*` variation cues.** They belong with the footstep/impact banks in phases B–D; authoring
  them now would be guessing at bank structure that phase B defines.
- **No reverb effect on `SM_GS_Reverb`.** The submix exists and is registered; the preset waits for a
  level with `AAudioVolume`s in it, which is phase G.
- **`GSGA_Horn.cpp` untouched.** Its three `TSoftObjectPtr<USoundBase>` fields are phase C's job; the
  wave-level routing here gets the horn onto the spine without opening a file phase C will rewrite.

**The circuit breaker was not hit** — the two failures (read-only submix parenting, subsystem loader
returning None) were each diagnosed and worked around on the second attempt with a different
approach, not retried.

> 2026-08-21T23:06Z Assets + ini done and read back from disk. NOT LIVE until an editor restart - AudioSettings is read at startup. Post-restart: assign surface_type on the 7 PM_GS_* materials, then the audible watch test.

### Late scope, taken deliberately (added after the first Evaluate)

Two things surfaced when I checked `git status` before handing back, and both were data-loss bugs
rather than tidiness:

1. **`Content/Audio/` was not tracked by git.** `.gitignore` ignores `Content/*` and opts folders
   back in one at a time; `Content/Audio/` had never been opted in. The three hand-authored horn
   SoundWaves - **the only project-authored audio that existed** - were untracked and would have
   vanished on a clean checkout. This is the *third* instance of exactly this bug: the file's own
   comments record `Content/AI/` (2026-08-04, "the entire defender AI layer was untracked and would
   have been lost") and `Content/Python/` + `Content/Data/` (2026-08-05). Added `!Content/Audio/`
   with a comment naming the pattern. Verified: `git status` now sees the folder, `git add -An`
   counts **51 files**, and `git check-attr filter` confirms **LFS applies** to the `.uasset`s.
2. **The 48 new assets were relocated** from `/Game/GoblinSiege/Audio/` to `/Game/Audio/`. Having
   two audio roots was wrong on its own, and consolidating meant the single `!Content/Audio/`
   negation rescues the horn and covers the spine at once. Moved via `EditorAssetSubsystem.
   rename_asset` (48/48, **zero redirectors left**), the now-empty source directory deleted, and the
   six `DefaultEngine.ini` paths rewritten to match. All cross-references re-read from the new
   locations afterwards: class→submix routes, `SM_GS_Music`'s effect chain, the duck preset's
   external submix, and the horn waves' class/attenuation. **`.gitignore` was not in my claimed file
   list** - `gsqueue check -Files .gitignore` returned CLEAR before I touched it, but it is scope
   taken outside the claim and belongs in the record.

**Not fixed, and not mine:** `Content/GoblinSiege/Test/` is also untracked. It looks like scratch,
but it is somebody else's, so I left it. Worth a glance - if it holds anything real, it is the
fourth instance of the same bug.

> 2026-08-24T02:18Z ICEBOXED by Michael 2026-08-23, not reverted - all work is committed in main. Handover with what is done and what is left: AgentQueue/ICEBOX.md. Status is abandoned only so it stops holding the build gate shut.
