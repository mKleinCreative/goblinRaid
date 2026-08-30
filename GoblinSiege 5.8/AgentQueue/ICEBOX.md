# Icebox

Work that was real, is committed, and is **not being worked right now**. Michael's ruling,
2026-08-23: park these, note what was done and where, so somebody can pick them up cold.

A ticket here is closed on the board as `abandoned` so it does not hold the build gate shut. **That
status is a lie about the work and a truth about the queue** - nothing was reverted, and everything
described below is in `main`. Read this file, not the status word.

To resume one: claim a fresh ticket, cite the old number, and start from "What is left" below.

---

## #249 - Audio phase A: the mixer spine

**Agent:** claude-audio. **Parked at:** `review`, unobserved, ~51h open.
**Where the work is:** committed. `Content/Audio/Mix/` - 51 tracked files.
**Ticket:** `AgentQueue/tickets/249-*.md` (its Refine section is the authoritative handover).

**Done and in the repo:**

- 22 sound classes under `Content/Audio/Mix/Classes/` (`SC_GS_Master` down through
  `SC_GS_Voice_Creature`, `SC_GS_SFX_Signal`, etc.)
- 8 submixes under `Content/Audio/Mix/Submixes/`
- Attenuation set under `Content/Audio/Mix/Attenuation/` (footstep, impact, foley, fire loop,
  ambience bed, structure, weapon swing, NPC voice, long signal)
- Concurrency set under `Content/Audio/Mix/Concurrency/`
- `Config/DefaultEngine.ini`: `DefaultSoundClassName` **and** `DefaultMediaSoundClassName`, plus a
  surface-type list carrying a permanence warning - appending a surface in the wrong alphabetical
  position silently repaints every material in the project. **Read that comment before editing it.**
- `always_play` set on `SC_GS_SFX_Signal`, deliberately: the horn is the one sound whose absence is a
  gameplay failure rather than a mix failure, and concurrency alone does not protect it from the
  64-voice cap.

**What is left (the ticket's own "deliberately left undone"):**

1. No physical-material painting pass. `L_Groatsworth` does not exist yet; everything resolves to
   Default -> Dirt until phase D. Michael's ruling was "footsteps are fine for now".
2. No `SC_GS_*` variation cues - they belong with the footstep/impact banks in phases B-D.
3. No reverb preset on `SM_GS_Reverb`. The submix exists and is registered; the preset waits for a
   level with `AAudioVolume`s, which is phase G.
4. `GSGA_Horn.cpp` untouched by design - its three `TSoftObjectPtr<USoundBase>` fields are phase C's
   job. **Note:** the horn work in #245 has since rewritten that ability's sound handling
   (`HornSoundStart/Loop/End`). Phase C should re-read the file rather than trust this line.

**Never observed.** Nobody has listened to the spine do anything. It is configuration that compiles
and loads; whether the mix sounds right is entirely unproven.

---

## #252 - Ruling: world corruption joins the slice

**Agent:** claude-corruption. **Parked at:** `review`, unobserved, ~48h open.
**Where the work is:** committed, in `docs/decisions-ledger.md` and `docs/goblin-siege-gdd.md`.
**Ticket:** `AgentQueue/tickets/252-*.md`.

**Done and in the repo:** the rulings block plus four GDD edits, including a §12.4 wayfinding
paragraph that would otherwise have contradicted the new corruption text three lines below it.
`check_gdd.py` reported CLEAN 10/10.

**Blocked on Michael, and this is the reason it stalled:**

> **Do civilian kills corrupt the world as much as knight kills do?**

The agent drafted this as ruling 46, then cut it on realising it would be recording a decision
Michael had never made. It is a flagged open question at the foot of the block. **Answering it is the
thing that unblocks stage 1.**

**What is left:**

1. The civilian-kill weighting question above.
2. `features.json` and the §12.1 row - deliberately deferred to ship together at stage 6.
3. Stages 1-6 of the plan. This ticket is stage 0: it authorises the work and builds none of it.
   Stage 1 needs an editor-closed build.

---

## World corruption - stages 5 and 6 (iceboxed 2026-08-27)

Michael's call: park it. **Stages 0-4 are BUILT, watched, and closed** - this is not the #252 icebox
warmed over, it is a working feature stopped four-sixths of the way through. Read the BUILT entry at
the top of `AGENT_STATE.md` before assuming anything here is unfinished.

**AGENT_STATE.md is the durable record**, by ruling. The 504-line design plan still exists at
`C:/Users/Michael/.claude/plans/i-want-you-to-abstract-newell.md` - machine-local, outside git, and
deliberately left there. Two passages in it are STALE and AGENT_STATE has the corrections: stage 6's
audio (it predates #249's mixer spine and describes a bare `UAudioComponent`), and stage 4 (which
split into "class shipped" and "asset not yet authored").

**Nothing was reverted and nothing is unbuilt.** Every corruption ticket closed properly; the code
compiled in the 18:15 build on 2026-08-27 and ran. The board is clear of corruption tickets, so it is
not holding the build gate.

### What actually remains

**Stage 4 is 90% done and the last 10% needs an editor, not a programmer.**
`UGSCorruptionDataAsset` exists and loads. `DA_Corruption_Default` does **not** exist yet, so every
run logs `No corruption tuning asset at '/Game/Data/World/DA_Corruption_Default...'` and falls back
to C++ defaults - **that warning is the designed behaviour, not a fault.** Creating the asset is a
content task. Two numbers in it are knowingly unfounded and want a watched raid, not a fresh guess:
- `KillSoftKnee` = 12, sized against ruling 19's 15-defender pool BEFORE the 2026-08-23 roster ruling
  made castle guards Militia with *"a decent amount of them"*.
- `CivilianKillWeight` = 2.5. Ruling 62 fixed the DIRECTION (civilians corrupt more) and deliberately
  not the magnitude.

The four `UCurveFloat` response slots on the asset are **declared and unconsumed** - the director
does not evaluate them yet. Shipping the slots without the wiring was deliberate and is recorded.

**Stage 5 - `MPC_GSCorruption` and the ground.** Untouched. The plan's write-up is still accurate.
The one piece that must be hand-authored is the `CollectionParameter` node into `M_GS_Crop_Master` -
assume Python cannot be trusted with a material graph, same risk class as the 2D blendspace. Do not
edit the Dreamscape landscape masters: marketplace content, shared across three maps, and ground char
already routes through `GS_BurnMask`.

**Stage 6 - ash, embers, ambience.** Untouched. `NS_AtmosphereAsh` / `NS_AtmosphereEmbers` in
`Content/VolcanoEnvironmentVFX/` - **their user-parameter names are unknown until someone opens
them.** Ambience routes through #249's spine (`Content/Audio/Mix/`), NOT the bare `UAudioComponent`
the plan describes. The GDD 12.1 row and the `features.json` entry must ship TOGETHER: `check_gdd.py`
pins ids to 1-20 + 5b and fails on both `missing` and `extra`.

### Unproven, and it owes a line

**#338 closed UNOBSERVED.** `GS.Corruption.Debug 0|1` registers its ticker and logs `live overlay
ON`, but the bar draws through on-screen debug messages that no log can confirm. **Nobody has looked
at the screen with it enabled.** If it turns out to draw nothing, the likely cause is the line filter
- it string-matches `DescribeState()`'s wording, so a reword silently empties the overlay.

### Debts carried, none blocking

- The kill log line is `Verbose`, so a real kill leaves no trace in the file. On 2026-08-27 that cost
  a round trip: a working hook could not be told from a dead one.
- A failed `Cast<AGSEnemyCharacter>` is silent - any human that is not one counts as a soldier.
- `ObjectiveRecomputeIntervalSeconds` and `CorruptionDataPath` are not in `DefaultGame.ini` (C++
  defaults apply; `#335` held that file at the time).
- Corruption hooks the three callers UPSTREAM of `UGSCrumbleComponent` rather than its `OnCrumbled`.
  #317 made Crumble the unified destroyed state; `GSBuildingObjective.cpp:684` crumbles directly and
  is not counted. Not broken - buildings feed the objectives term - but the wrong shape.

### The open design question

With per-TYPE weighting, burning one house of 67 moves the objectives term by almost nothing. Should
razing an entire street feel like an achievement? Today it reads mainly through the structures term,
which has its own knee. Nobody has decided this.

## #356 - Horn blast raise: slerp arm ramp into confirmed frame-29 hold pose (iceboxed 2026-08-29)

**Agent:** claude-anim. **Parked at:** `blocked`, ~2.9h open.
**Where the work is: UNCOMMITTED.** Unlike every other entry in this file, this one is NOT in
`main` - `git status` shows both files still modified in the working tree. Read this file for
context, but do not assume the described state is on disk in `main`; verify against the actual
asset before resuming.
**Files:** `Content/Characters/ScoutV2/Anims_LocoSet/A_GS_HornBlow_Gob.uasset`,
`Content/Characters/ScoutV2/Montages/AM_GS_HornBlast_Intro.uasset`.
**Ticket:** `AgentQueue/tickets/356-*.md` - its body is the authoritative blow-by-blow (three
separate rounds of fixes, each one caught wrong by Michael watching live).

**Done, in the working tree, not yet committed:**

1. Left-arm raise (8 bones: `l_clavicle`, `l_upperarm`, `l_upperarmtwist01/02`, `l_forearm`,
   `l_forearmtwist01/02`, `l_hand`) re-keyed as a quaternion slerp instead of the leftover
   `A_MX_Taunt_Battlecry_Gob` flourish motion the source clip carried.
2. `AM_GS_HornBlast_Intro`'s `blend_out_time` shrunk 0.25s -> 0.05s, to stop an auto-blend-out-to-
   idle dip between the Intro and Loop montages (confirmed real via `AnimMontageService`, not
   guessed).
3. Michael manually posed and keyed a corrected arm position directly in the editor ("Add Key",
   which lays down an ADDITIVE track) - this pose is the real ground truth for the eventual fix,
   and per-frame-diff it lands the hand ~43 units from `socket_mouth` (down from ~150-164 units
   before), the closest approach at **frame 50**.

**What is left, and why it stalled:**

1. **"Flatten the loop at frame 50" did not work.** Three different technical approaches
   (`apply_bone_rotation` per-frame loop, the same API's native frame-RANGE mode, and an isolated
   single-bone/single-script test) all reported `success: true` but produced smooth interpolation
   on read-back instead of a flat plateau. Root cause unconfirmed - working theory is the asset's
   `ACLAnimBoneCompressionSettings` compression (13:1 ratio, error thresholds at 0.0) is
   reprocessing repeated `apply_bone_rotation` calls rather than landing them as independent keys,
   but this was never verified. **Do not assume the Python tooling can pin a flat hold on this
   asset without checking that theory first.**
2. **"The beginning is snapping" (Michael's second report) is unresolved** and was never
   root-caused past a hypothesis (Intro's raise not arriving at the same value Loop holds at,
   post-additive-key). The one clean fix that DID work earlier in the ticket (re-ramping the raise
   to arrive exactly at the hold value by frame 24) has not been redone against the NEW frame-50
   target.
3. `socket_mouth` (the real ground-truth mouth position, added by Michael on `GOB_Scout_v2_Skeleton`
   after his own placed marker + this position disagreed) is the correct reference for any future
   attempt - use it directly rather than re-deriving an estimate.
4. Likely resume path Michael suggested but never executed: flatten frames 24-48 by hand in the
   editor (he now knows where "Add Key" lives), since the Python-side flatten is the piece that's
   actually broken.

**Never observed working end-to-end.** Every round of this ticket was watched live by Michael and
found wrong in a NEW way each time (neck clipping, double-start, still-snapping) - there is no
point at which the raise-to-hold-to-lower sequence has read as correct.

## #269 - closed UNOBSERVED (not iceboxed, recorded here because it owes a line)

ACF Phase 3 scoping - why the project carried an unused `UACFEquipmentComponent` since Phase 2a
while `UGSWeaponComponent` did the same job.

**What is unproven:** nothing was run, because nothing runnable was produced - it is analysis, not
code. Its conclusions were acted on in #270, which *was* observed (10/10 goblins equipping through
ACF). If #269's reasoning turns out to be wrong, the visible symptom will be in #270's behaviour,
not in anything #269 itself shipped. The two components still coexist deliberately; #270 changed
nothing about `UGSWeaponComponent`.
