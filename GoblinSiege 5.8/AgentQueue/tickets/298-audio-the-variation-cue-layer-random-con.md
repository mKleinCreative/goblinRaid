---
id: 298
title: Audio: the variation cue layer - random containers with pitch/volume jitter over the pack waves
agent: claude-audio
status: done
claimed: 2026-08-24T23:46Z
build: none
waiting_on:
evaluated: 2026-08-25T02:18:06Z
observed: 2026-08-25T02:17:35Z | Michael listened to the cue layer through the console auditions and signed it off - it all sounds good. Specifically he heard six goblin barks read as a group rather than one voice repeating (his words: the modulation sounded like a group barking), which is the pitch-band variation on SW_Humanoid_Small_1 working, since the pack ships that goblin as a single performer. He asked for laughs mixed through the crowd; horde() now fires six barks plus two Glee cues and he heard that too.
scenario: PIE running on L_Tutorial_Island, cues fired from the console via py import gs_audio then gs_audio.horde() and gs_audio.sampler(), on headphones.
files: 
  - Content/Audio/Cues
  - Config/DefaultEngine.ini
  - Content/Python/gs_audio.py
---

## Goal

Audio: the variation cue layer - random containers with pitch/volume jitter over the pack waves

## Generate

**41 SoundCues** under `Content/Audio/Cues/`, built with VibeUE's `SoundCueService`. The pack ships
1,052 cues that are **1:1 wrappers with no randomisation** - its "variation" is ten separate assets,
not one cue that picks between them - so this layer is what turns the library into something a game
can actually trigger. Every vendor asset is left untouched; these wrap the pack's *SoundWaves*.

Each cue is the same shape: **Modulator (root) → Random → N wave players**, with per-play pitch and
volume jitter on the modulator, and sound class / attenuation / concurrency assigned from the phase A
spine.

| Group | Cues | Notes |
|---|---|---|
| Footsteps | 4 | Dirt/Grass × Walk/Run, 10 waves each, on `ATT_GS_Footstep` + `CC_GS_Footsteps` |
| Combat impacts | 6 | Flesh, Armor, Blunt, Block_Metal, Block_Wood, Body_Fall |
| Swings & bow | 5 | Swing_Light, Swing_Heavy, Bow_Draw, Bow_Release, Arrow_Impact |
| Gear foley | 2 | Weapon_Draw, Armor_Foley |
| Goblin voice | 5 | Bark, Pain, Death, Glee, Player |
| Human voice | 4 | Pain, Death, Alert, Effort |
| World | 7 | Door_Open/Close, Gate_Open, Chest_Open, Break_Wood, Break_Glass, Rock_Impact |
| Loot / fire / blast | 4 | Loot_Coins, Loot_Bag, Fire_Whoosh, Explosion |
| UI | 4 | Confirm, Tick, Refuse, Objective |

**The sword pulls from a pool, deliberately.** Only three `Sword_Hit` waves exist in the entire pack,
for the game's primary weapon, so `SC_GS_Hit_Flesh` draws from `Generic_Hit` + `Stab` (12 waves) and
`SC_GS_Hit_Armor` from `Metal_Weapon_Clash` (8).

**The goblin pitch bands are the point, not decoration.** `SW_Humanoid_Small_1` is **one performer**,
so without per-play pitch variation ten goblins sound like one goblin with an echo. Bands:
Bark 1.00-1.14 · Pain 0.96-1.10 · Death 0.94-1.08 · Glee 1.02-1.16 · **Player 0.86-0.96**, the last
deliberately below the horde so the pawn you control does not read as one of the crowd. All voice
cues carry **both** `CC_GS_Voice_PerActor` (one voice per owner) and `CC_GS_Voice_Global`.

**Two things outside the original scope, both to make the work testable:**

1. `Config/DefaultEngine.ini` gains `[Audio] UnfocusedVolumeMultiplier=1.0`. The engine default
   (`BaseEngine.ini`) is **0.0**, which silences PIE the instant the window loses focus - so every
   listening test had to be done click-focused in the viewport, and sounds fired from a tool were
   inaudible. This is why the first audition attempt produced "didn't hear them".
2. `Content/Python/gs_audio.py` - console-driven audition: `py import gs_audio` then
   `gs_audio.bark()`, `gs_audio.horde()`, `gs_audio.sampler()`, `gs_audio.play("Hit_Flesh", 4)`,
   `gs_audio.ls("Vox")`. Michael asked for this directly; audio cannot be verified by read-back, so a
   trigger he can drive without an agent in the loop is infrastructure, not convenience.

## Evaluate

**Verified:** all 41 cues audited programmatically after saving - every one has a Modulator root, a
Random node whose child count equals its wave-player count, a non-null sound class, and a non-zero
duration. Zero problems across 41. `gs_audio` was imported and exercised in the editor: `ls("Vox")`
returned the 9 voice cues and `play()` resolved and fired a cue.

**Verified by ear (updated after the audition).** Michael listened and signed the layer off - "it all sounds good" - and confirmed the one thing that could not be reasoned about: **the six-bark horde reads as a crowd rather than as one voice repeating.** That validates the pitch bands, which were pure inference until someone heard them, and it is the whole justification for this cue layer existing over the pack's 1:1 wrappers.

**Still not individually judged:** the *contents* of each cue. The audition was a group listen, not a cue-by-cue review, so a mis-matched substring could still be hiding a plausible-sounding cue full of the wrong material. Relative levels between groups are also untrimmed - nothing has been balanced against anything else, because that is a mixing pass against real gameplay rather than a bench test.

**The wave selection is by substring match and is the weakest part.** `pick()` matches names like
`"Whoosh_"` excluding `"Metal"`. That is fast and got 41 cues built, but it means the *contents* of
each cue are unaudited - a mis-matched substring silently yields a plausible-looking cue full of the
wrong sound. Wave counts were printed per cue and all looked sane (3-12), but "sane count" is not
"right sounds". `SC_GS_Break_Wood` (matching `Wood_0`/`Wood_1` in Environment) and
`SC_GS_Break_Glass` (3 waves) are the two I would check first.

**A limit worth stating plainly:** `SC_GS_Vox_Gob_Bark` and `SC_GS_Vox_Gob_Pain` draw from the *same*
`_-_Short_` waves and are distinguished only by pitch band. That is honest given one performer, but
it means an aggro bark and a pain grunt may not read as different events. If they don't, the fix is
splitting the Short set by ear rather than widening pitch further.

**Owed to AGENT_STATE.md - a DECISION line:** *the audio cue layer is `Content/Audio/Cues/SC_GS_*`,
41 random-container cues over the pack's waves; the pack's own 1:1 cues are not used and its assets
are not modified. `[Audio] UnfocusedVolumeMultiplier=1.0` so PIE keeps sounding when unfocused.*

## Refine

**Changed during the pass:**
- Built one cue as a prototype and read its full node graph back before batching the other 40, rather
  than generating 41 and discovering a systematic wiring error at the end.
- Added `SC_GS_Vox_Gob_Player` after realising the plan's "give the player a different pitch band"
  note needed its own asset, not a runtime parameter - the pawn and the horde pull different cues.
- Diagnosed the failed audition properly instead of building a workaround. Michael said he could not
  hear sounds unless clicked into PIE; the reflex would have been to script around it, but the cause
  was a one-line engine default, and fixing it helps every future audio test rather than just this one.

**Deliberately left undone:**
- **No fire/ambience loop cues.** Those need a Looping node and belong with the emitters in phase G;
  authoring them now would guess at how the burn system wants to drive them.
- **No footstep cues for stone/wood/thatch.** Michael ruled footsteps "fine for now" - dirt and grass
  are all the pack has.
- **No per-cue volume balancing.** Levels are all at the modulator's 0.85-1.0 jitter with no relative
  trim between groups. That is a mixing pass done by ear against the real game, not a guess made now.

**Open, and blocking closure: this ticket has not been heard.** It should not close until Michael has
run `gs_audio.horde()` and `gs_audio.sampler()` and judged them - and the unfocused-volume fix needs
an **editor restart** before he can.

> 2026-08-24T23:55Z Built and structurally audited. NOT heard yet - needs an editor restart for the unfocused-volume fix, then gs_audio.horde() and gs_audio.sampler().

### Heard and signed off, 2026-08-24

Michael listened through the console auditions: **"it all sounds good."** The specific confirmation
that mattered - **the six-bark horde reads as a crowd, not as one voice repeating** ("the modulation
sounded like a group barking"). That was the riskiest assumption in this ticket, because the pack
ships `SW_Humanoid_Small_1` as a **single performer**, and the pitch band was chosen by reasoning
with nothing to check it against. The band widths stand: Bark 1.00-1.14, Player 0.86-0.96.

He also asked for **laughs mixed through the crowd**, which is now in `horde()` - six barks plus two
`Vox_Gob_Glee`. **That ratio is a design answer, not a test helper:** a crowd that only barks reads
as wolves, and the laughter is what makes them goblins in a game that is explicitly satire. **The
horde bark logic in phase C should use roughly the same 1-in-4 ratio**, and this is the note that
carries that decision forward rather than leaving it in a throwaway script.

Still true and not closed by this sign-off: the *contents* of each cue were chosen by substring match
and only spot-checked by ear as a group. If `Break_Glass` (3 waves), `Break_Wood` (loosest match) or
`Hit_Flesh` (pooled `Generic_Hit`+`Stab`, because the pack has only 3 real sword-hit waves) turn out
wrong in real gameplay, re-picking the waves is a data edit against a cue that is already wired.
