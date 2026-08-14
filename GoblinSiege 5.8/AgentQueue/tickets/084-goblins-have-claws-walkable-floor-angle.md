---
id: 084
title: "Goblins have claws: walkable floor angle 62 -> 65, which is also the climb/walk boundary"
agent: claude-climbrebuild
status: done
claimed: 2026-08-08T21:26Z
build: none
waiting_on:
evaluated: 2026-08-08T22:10:00Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Michael, on the roof the ledge search could not solve: *"Medium_02's roof looks more walkable than
climbable, but that should be the limit on what you can actually climb. Think that goblins have claws
on their feet and should be able to be a little more agile climbing tree's and buildings than other
people."*

## Generate

`BP_GSPlayerCharacter` -> `CharacterMovement` -> **`WalkableFloorAngle` 62 -> 65 degrees**
(`WalkableFloorZ` 0.4695 -> 0.4226). One property. No build - it was never set in C++, only on the
Blueprint CDO, so the whole change is editor-side.

Nothing else needed changing: `UGSClimbLibrary::FindClimbLedge` already reads
`GetWalkableFloorZ()` off the character rather than a constant, so the ledge search inherits this
automatically.

## Evaluate

**Measured on the 20-house corpus, before and after:**

| WalkableFloorAngle | roof lips resolved | still failing |
|---|---|---|
| 62 (Z 0.4695) | 76 / 80 = **95%** | Medium_02 E, Medium_02 W, Small_8 N, Small_10 E |
| **65 (Z 0.4226)** | **78 / 80 = 98%** | Small_8 N, Small_10 E |

**Roof continuation drops from four cases to two**, and the two that remain are genuinely steep -
`Small_8` at **82.5 deg** and `Small_10` at **79.6 deg** - not marginal ones.

**65 was chosen for its margins, not because it is a round number.** Michael's rule is that
Medium_02's roof is exactly the boundary, so the gate has to sit just past it:

| surface | angle | walkable at 62 | at 65 |
|---|---|---|---|
| Medium_02 roof | 63.3 | no | **yes** |
| fascia | 74.9 | no | no |
| fascia / steep roof | 79.6 | no | no |
| Small_8 roof | 82.5 | no | no |

1.7 deg of margin on the accept side and **9.9 deg on the reject side** - the fascias that caused the
"it bumped me off" bug in #079 are nowhere near becoming walkable.

**This is a locomotion change, not just a climbing one, and that is the point.** Goblins can now walk
up any 65-degree slope anywhere in the world - terrain, rubble, roofs - which is the "claws" trait
Michael described. It should be judged on how the goblin handles hills, not only on Medium_02.

**NOT PLAYED.** The corpus numbers are trace simulation; the CDO change was read back from a fresh
asset load rather than trusted from the setter. Whether 65 feels agile-but-not-silly is Michael's call.

**Deliberately not done as a climb-only gate.** The tempting fix was a separate, looser threshold
inside `FindClimbLedge` so it would mantle onto surfaces the character cannot stand on. That would
have put the goblin on a 63-degree roof and let the movement component immediately slide him off -
the same class of bug as #079's fascia mantle. Michael's framing (walkable *is* the limit) is the
correct model and it is one number instead of two.

**Owed AGENT_STATE.md** - DECISION (2026-08-08, Michael): goblins have claws. `WalkableFloorAngle` is
**65 degrees**, and that angle is simultaneously the steepest walkable surface and the boundary above
which a surface must be climbed rather than walked.

## Refine

- **Put the change on the character, not in the ledge search.** One property now governs both "can I
  stand here" and "is this a ledge worth mantling onto", so they cannot drift apart.
- **Checked the reject-side margin, not just that the target case passed.** The accept case only needed
  63.3; the question that mattered was whether 65 also lets a 74.9-degree fascia through. It does not.
- **Read the value back from a fresh `load_asset`**, not from the object just written - this project
  has repeatedly hit setters that report success and change nothing.
- **Did not touch C++ or request a build.** #083 (claude-npcfight) is active and holds the gate; this
  change does not need it.

**Deliberately left undone:** roof continuation for `Small_8` and `Small_10`, which are genuinely too
steep to stand on and need the climb to continue onto a pitch; per-goblin cadence jitter for horde
climbing, parked at Michael's request until the combat/summoning window (#083) lands; Stage 4 plane
transition, still awaiting his observation.
