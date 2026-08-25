---
id: 297
title: Horn pops: CC_GS_Signal max 1 denies the horn's own end sample mid-fade
agent: claude-audio
status: done
claimed: 2026-08-24T23:37Z
build: none
waiting_on:
evaluated: 2026-08-24T23:41:22Z
observed: 2026-08-24T23:41:23Z | Michael blew the horn on MMB with music playing. With the cap raised to 3 per-owner the pop was unchanged, so the concurrency was not the cause. Stripping sound class, attenuation and concurrency off all three horn waves and blowing again still popped - so the artifact is pre-existing in the horn's own start/loop/end handoff and no part of the audio spine produces it. Routing restored afterwards.
scenario: PIE on L_CombatArena, combat music looping through SC_GS_Music, horn triggered by hand on middle mouse, tested before and after the change and again with all routing removed.
files: 
  - Content/Audio/Mix/Concurrency
---

## Goal

Horn pops: CC_GS_Signal max 1 denies the horn's own end sample mid-fade

## Generate

Michael reported a popping artifact on the horn blast, found while confirming the phase A music
duck. One asset changed: `Content/Audio/Mix/Concurrency/CC_GS_Signal`.

**Before:** `max_count = 1`, `PREVENT_NEW`, global.
**After:** `max_count = 3`, `STOP_OLDEST`, `limit_to_owner = true`.

The reasoning: one horn blast is **up to three overlapping voices by design**, and `GSGA_Horn.cpp`
says so explicitly. `StopHornVoice` (`.cpp:321-348`) spawns the End sample *while* the Loop is still
fading out over 0.06s, and on a short tap it deliberately leaves the attack running underneath -
its own comment reads *"The attack is deliberately NOT cut. On a tap short enough that the sustain
never started, clipping it would swallow the whole horn and the player would hear a click for their
press."* A global cap of 1 with `PREVENT_NEW` therefore denies the horn its own end sample and cuts
the fade the code just set up. `limit_to_owner` keeps the original intent - one blower cannot be
stacked on by another - while letting a single blast own its three stages.

## Evaluate

**The fix did NOT fix the reported symptom, and I should say that first.** Michael re-tested with
`max 3 / per-owner` and reported the pop *"Unchanged - pops exactly as before."* My diagnosis was
wrong.

**So I stopped tuning parameters and isolated instead**, which is the part worth keeping. Rather
than guessing at a second cause, I stripped **all three** of my phase A properties off the horn
waves - `sound_class_object`, `attenuation_settings` and `concurrency_set` all cleared - returning
them to their exact pre-phase-A state, and had him blow the horn again in a fresh PIE session.
Result: **"Still pops."**

That is a clean A/B, and it settles the question that actually mattered: **nothing in the audio
spine causes this pop. It is pre-existing in `GSGA_Horn`'s three-stage handoff and predates every
asset I have authored.** The routing has since been restored and re-read from disk.

**What is verified:** the concurrency values, read back from disk after saving; the horn's restored
class/attenuation/concurrency, read back from disk; and both listening tests, done by Michael on
`L_CombatArena` with music on the spine.

**What is NOT verified:** that `max 3 / per-owner` is *correct* rather than merely *not wrong*.
Nobody has heard a case where the old cap of 1 audibly denied a voice, because the pop that prompted
the investigation turned out to be something else. The change stands on the code comment in
`GSGA_Horn.cpp`, not on an observation. It is a latent-bug fix, not a heard one, and this ticket
should not be read as evidence that the horn's voice handling is now proven good.

**What I got wrong, and the cost:** I asserted a cause from reading code and changed an asset before
testing the assertion. The A/B I ran second should have been the first thing I did - it took one
round trip and answered the question completely, whereas the fix-first approach cost a save, a PIE
restart and one of Michael's listening tests to learn nothing. This is exactly the failure the
project rules warn about.

**Owed to AGENT_STATE.md - a FAILED line:** *the horn's audible pop is not the audio spine; it is
pre-existing in `GSGA_Horn`'s start/loop/end handoff, proven by A/B with all routing stripped
(#297, 2026-08-24).*

## Refine

**Kept the change**, and the reason is not "it might help": a sound the engine is told may only have
one voice, whose own code deliberately overlaps three, is a real defect whether or not it is the
defect Michael heard. Reverting it would restore a cap that will deny a voice the moment two horn
stages coincide under load. It is now justified on the code, and the ticket says plainly that it was
not the reported bug.

**Audited the sibling concurrency assets for the same mistake** - a cap set below the number of
voices its sound actually uses. `CC_GS_Ambience` is also `max 1 / PREVENT_NEW`, which is correct
there because an ambience bed is one looping voice per sound and is scoped `PerSound`. The rest
(Footsteps 10, Impacts 12, Voice_Global 5, Fire 8, Structure 6, UI 4, Default 32) are all well above
any single sound's voice count. `CC_GS_Voice_PerActor` at 1 is deliberate and stays.

**Deliberately left undone: the pop itself.** It is pre-existing, it is in C++, `GSGA_Horn.cpp` is
not in my claim, and the horn is scheduled to be rewritten in **audio phase C** when it migrates
from `TSoftObjectPtr<USoundBase>` fields to tag-posting. Fixing the handoff now means editing a file
that phase C rewrites. The likely suspects for whoever takes it - recorded so the next agent does not
re-derive them - are the `GetDuration()`-timed start→loop handoff drifting by a buffer at the seam,
and the 0.06s `FadeOut` being short enough to leave a step rather than a fade. Neither has been
measured; that is the phase C job, and it should start with an instrument on the seam, not a guess.
