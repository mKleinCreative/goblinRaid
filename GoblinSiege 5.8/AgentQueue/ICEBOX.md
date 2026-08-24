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

## #269 - closed UNOBSERVED (not iceboxed, recorded here because it owes a line)

ACF Phase 3 scoping - why the project carried an unused `UACFEquipmentComponent` since Phase 2a
while `UGSWeaponComponent` did the same job.

**What is unproven:** nothing was run, because nothing runnable was produced - it is analysis, not
code. Its conclusions were acted on in #270, which *was* observed (10/10 goblins equipping through
ACF). If #269's reasoning turns out to be wrong, the visible symptom will be in #270's behaviour,
not in anything #269 itself shipped. The two components still coexist deliberately; #270 changed
nothing about `UGSWeaponComponent`.
