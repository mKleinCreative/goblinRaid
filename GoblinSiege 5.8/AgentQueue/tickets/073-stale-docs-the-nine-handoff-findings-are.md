---
id: 073
title: Stale docs: the nine HANDOFF findings are already fixed, the run-start banner says otherwise; record the horde decision lines
agent: claude-horde
status: done
claimed: 2026-08-07T19:56Z
build: none
waiting_on:
evaluated: 2026-08-07T20:06:05Z
files: 
  - AGENT_STATE.md
  - HANDOFF.md
---

## Goal

Stale docs: the nine HANDOFF findings are already fixed, the run-start banner says otherwise; record the horde decision lines

## Generate

Seven edits across two files. No code.

**`HANDOFF.md`** — PART 1's heading changed from "OUTSTANDING CODE-REVIEW FINDINGS (nobody has fixed
these)" to "ALL NINE FIXED", with a block naming which ticket closed what: #036 took four (finding 2
the `set -Status done` bypass, finding 3 the stale-Evaluate gate, finding 9 the truncated `evaluated`
stamp, and it correctly *declined* finding 1 because #033's building rewrite had already deleted the
crashing code); #037 took the five C++ ones. The nine findings themselves are kept as the record of
what was found and why.

**`AGENT_STATE.md`** —

1. The run-start banner rewritten. It said `HANDOFF.md` holds "nine outstanding code-review findings
   that nobody has fixed". That banner is the first thing every agent reads at run start.
2. NEXT: the "nine outstanding code-review findings" item struck, pointing at #036/#037.
3. NEXT: "Arrows ignore RaceTag" struck as **false, and false when written** — #038 landed it, and
   what arrows still do (stick in an ally, dealing nothing) is Michael's settled 2026-08-06 ruling.
4. NEXT: the horn & horde line rewritten from `[BLOCKED] ... missing: UGSHordeSubsystem,
   AGSHordeSpawnMarker, UGSGA_Horn, BT_HordeGoblin` to `[EDITOR]` with the C++ half done and
   PIE-verified, the remaining editor work enumerated, and `AGSHordeSpawnMarker` marked as something
   that will never be built.
5. DECISIONS: a new dated block carrying all sixteen of Michael's horde rulings.
6. The `Content/*` is gitignored claim corrected.
7. The UBA / ~6-minute build note corrected.

## Evaluate

**Every claim I struck, I verified first rather than trusting the audit that surfaced it.** That
mattered — the audit was right here, but its whole value was in re-checking someone else's stale
list, and repeating that mistake in the other direction would have been worse than leaving it.

- The nine findings: read #036's and #037's own Generate sections, which enumerate what each fixed.
  Two I confirmed by hitting them directly this session — `set -Id 070 -Status done` was REFUSED
  with the message citing ticket 028, and my own tickets stamped `evaluated:` with seconds.
- Arrows: `GSArrowProjectile.cpp:150` calls `ShooterChar->IsHostileTo(OtherActor)` and gates damage
  on it. Read the file.
- `Content/` tracking: `git ls-files "GoblinSiege 5.8/Content"` returns **465** files, and
  `git ls-files --error-unmatch` resolves `BP_GSPlayerCharacter.uasset`, `BT_Militia.uasset` and
  `IMC_Default.uasset` individually. The `.gitignore` writes `Content/*` and then re-includes the
  project folders with `!Content/Characters/` etc. The old note read the ignore line and not the
  negations.
- UBA: `Build-GoblinSiege.ps1` printed "UBA cache is writable - parallel compilation available" and
  the build ran 6 actions across 12 physical cores in 4:45 (#069). Evidence, not inference.
- The horn key: read the live `IMC_Default` default key mappings rather than the audit's summary —
  17 rows, 17 distinct keys, no duplicates, `G -> IA_Block` and `F -> IA_Interact`. This is why
  Michael's "move it to F" got questioned instead of implemented.

**What is written but has never been read by anyone:** all of it. These are documents; the test is
whether the next agent behaves differently, and that cannot be verified from inside this session.

**Touched outside the goal:** nothing. Both claimed files, no code.

**The honest limit of this ticket:** it fixes the instances, not the mechanism. A finding lives in
the ticket, in `HANDOFF.md`, and in `AGENT_STATE.md`; closing the ticket closes one of three, and
nothing forces the other two. #036 and #037 both did good work and both left the documents
advertising that work as outstanding. That will happen again. A `gsqueue.ps1 done` check that asked
"does any doc still list this as open?" would be the real fix, and it is not in this claim.

## Refine

Changed in response to my own evaluation: the banner text grew a "the lesson" paragraph rather than
just being corrected. A silently-fixed banner would have left the next agent with no idea the
project has a recurring failure mode here, and the correction is more useful than the fact.

Deliberately left undone: the `gsqueue.ps1` close-check described above. It is a real fix and it is
a different ticket touching a different file, with its own design question (how does a script know a
markdown bullet refers to the finding a ticket closed?). Raising it rather than bolting something
fragile onto this one.

Also left: `GoblinSiege 5.8/CLAUDE.md` still carries the ~6-minute UBA-crippled build note in its
"Known performance issue" section. Not in this claim, and it is a generated-in-part file (the VibeUE
half), so it wants its own pass — noted here so it is not lost.
