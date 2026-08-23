---
id: 252
title: Ruling: world corruption joins the slice (scope freeze amendment)
agent: claude-corruption
status: review
claimed: 2026-08-22T02:14Z
build: none
waiting_on: Docs only, no build. Rulings 40-45 in the ledger + four GDD edits; check_gdd.py CLEAN 10/10. Needs Michael on one open question: do civilian kills corrupt the world as much as knights? features.json and the 12.1 row deliberately deferred to stage 6.
evaluated: 2026-08-22T02:18:15Z
observed: 2026-08-22T02:17:59Z | Ran the drift gate against the two edited documents: check_gdd.py reported CLEAN, 10 of 10 checks, exit 0 - with systems_inventory still reading 21 ids, which is the check that would have failed had I added a 12.1 row without its features.json entry
scenario: python Tools/check_gdd.py -v from the repo root, after both the ledger block and the four GDD edits were in place
files: 
  - docs/decisions-ledger.md
  - docs/goblin-siege-gdd.md
  - Tools/CodeArchitect/features.json
---

## Goal

Ruling: world corruption joins the slice (scope freeze amendment)

## Generate

**`docs/decisions-ledger.md`** — new dated block `## 2026-08-21 — the land turns as you raid (world
corruption)`, inserted above the Warren block of the same date, carrying **rulings 40–45**:

- **40** world corruption joins the slice: one global 0..1 scalar over sky/fog/sun/grade/materials/VFX/ambience
- **41** monotonic, never recedes
- **42** it is NOT the alarm meter and NOT derived from deeds
- **43** output stage is hand-lerped C++, not Epic's DaySequence — with the reason recorded so it is not re-investigated
- **44** ACF contributes nothing; no ACF module joins `GoblinSiege.Build.cs`
- **45** per-raid, no cross-raid persistence

Numbered 40+ because today's block already ran 36–39. The ledger carries **two overlapping
sequences** (the 2026-08-20 ACF block also reaches 36) — I read the tail before numbering rather
than taking the file maximum.

Plus one item recorded as an **open question, not a ruling**: the race test is
`GSCharacterBase.cpp`'s existing *"anything that is not a goblin is a human here"*, so with
civilians IN (13/14) a slaughtered peasant would corrupt the world exactly as much as a knight.
Nobody has decided that. The ledger's own rule — *"an entry with no source is not a ruling, it is a
recollection"* — is why I did not invent one.

**`docs/goblin-siege-gdd.md`** — four edits:

1. Revision history row `v1.1 | 2026-08-21 | #252`
2. §1 pillar reworded: *"fire and destruction as a language, **written on the land itself** (40)"*
3. §12.4 scope-freeze IN column gains *"World corruption — the land turns as you raid (40)"*
4. §12.4's wayfinding-consequence paragraph amended — it stated that deferring wayfinding leaves
   objective names as the player's only guidance. Ruling 40 is the answer to it, so the paragraph
   now says so.

**`Tools/CodeArchitect/features.json`** — claimed, deliberately **not edited**. See Evaluate.

**Calls made:** `gsqueue claim` → #252 (no conflicts) → `set -Status active` →
`python Tools/check_gdd.py -v`.

## Evaluate

**Verified by running the gate.** `python "GoblinSiege 5.8/Tools/check_gdd.py" -v` → **exit 0, 10/10
checks**, including `banked_vocabulary - 3 file(s) clean` (I introduced no retired vocabulary) and
`roster_matches_generator`. Before/after is meaningful here: this ticket edits the two documents the
gate reads, and the gate is the only executable consequence a docs change has.

**The load-bearing line is `systems_inventory - 21 ids`** — unchanged. That is the check I was most
at risk of breaking. `EXPECTED_IDS` is `1..20 + 5b`, pinned against `features.json`, and it fails on
**both** `missing` and `extra`. Adding a §12.1 row for corruption without a matching feature entry
would have failed the build with exit 5, and adding both here would have put a `features.json`
entry in the tree describing code that does not exist — which is the *same* class of lie the file
exists to catch (its docstring: the generator went on enforcing a building the design no longer
contained). **The §12.1 row and the `features.json` entry ship together at stage 6, when there is
something to grade.** So `features.json` stays claimed-but-untouched, and this ticket is honestly
narrower than its file list.

**Adversarially, what is weak:**

- **Ruling 42 rests on two facts I checked myself, not on a subagent's word.** `Alarm` really is a
  sawtooth (`GSGameState.cpp:82` sets `Alarm = MaxAlarm * PostHordeResetFraction`, `.h:239` = 0.4),
  and no MPC asset exists (`find Content -iname "MPC_*" -o -iname "*ParameterCollection*"` → empty).
  An earlier survey claimed ~20 Dreamscape materials already reference an MPC; that was
  `ParameterCollectionInfos`, a field serialized into every `UMaterial`, which hits 172 files
  including our own `M_GS_AimArc`. Both premises were wrong in the first draft of the plan and were
  corrected before this ruling was written.
- **Ruling 43 is a judgement call, not a fact**, and it is the one most likely to be revisited. It
  turns on `L_CombatArena` having no `ADaySequenceActor` — true today, and false the moment someone
  hand-authors a level. That is why it says *"revisit"* rather than *"rejected"*.
- **Nothing here has been watched in PIE, because nothing here runs.** No code was written, no build
  was attempted, and the build gate is CLOSED (12 tickets open) so no build was possible. Every
  behavioural claim in the plan this ruling authorises is still unobserved.
- **Ruling 41's second half is the one worth challenging later.** I assert the burn mask's R channel
  is monotonic by construction and therefore a falling global scalar would render a clean sky over
  black ground. That is read off `GSBurnMaskSubsystem`'s design, not watched. If it turns out the R
  channel decays after all, 41 loses its correctness argument and keeps only the §8 doctrine one.

**Owes `AGENT_STATE.md`:** a DECISIONS line — *"World corruption is a system (rulings 40–45), it is
monotonic, and it is deliberately not the alarm meter."*

**Outside the goal:** nothing. Three files claimed, two edited, one deliberately left alone.

## Refine

**Changed in response to my own evaluation:** the §12.4 wayfinding paragraph was not in my original
edit list. Writing the Generate section made it obvious that adding corruption to the IN column
while leaving a paragraph three lines below still saying names are the player's only guidance would
be exactly the document-drifts-from-the-build failure this GDD's change rule exists to prevent. So
the paragraph was amended in the same pass.

I also **cut a ruling I had drafted** — a civilian-kill weighting decision. I had written it as
ruling 46 before noticing I would be recording Michael's decision without Michael having made one.
It is now the flagged open question at the foot of the block, which is the honest shape.

**Deliberately left undone:**

- **`features.json` and the §12.1 row** — ship at stage 6, together, per the reasoning in Evaluate.
- **The civilian question** — Michael's to answer, surfaced to him rather than assumed.
- **Everything else in the plan.** This ticket is stage 0 of 6 and authorises the rest; it builds
  none of it. Stage 1 needs an editor-closed build, which the closed gate forbids today.
