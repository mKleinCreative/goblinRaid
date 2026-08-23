---
id: 255
title: Loot banks at the beginning portal too: one banking component, given to the runic site
agent: claude-warren
status: done
claimed: 2026-08-23T00:27Z
build: required
waiting_on:
evaluated: 2026-08-23T22:42:31Z
observed: 2026-08-23T22:42:46Z | Michael watched loot bank at the runic site - the portal now takes cargo the way the Warren does. First time this code path has ever executed.
scenario: PIE with a runic site present, editor build of 2026-08-23 15:38.
files: 
  - Source/GoblinSiege/Raid/GSLootBankComponent.h
  - Source/GoblinSiege/Raid/GSLootBankComponent.cpp
  - Source/GoblinSiege/Raid/GSRunicSite.h
  - Source/GoblinSiege/Raid/GSRunicSite.cpp
---

## Goal

Loot banks at the beginning portal too: one banking component, given to the runic site

## Generate

**`Raid/GSLootBankComponent.{h,cpp}` (new)** - one banking implementation, so the runic site does not
grow a second copy of the Warren's. GDD 9 asked for exactly this: *"The Warren needs the same seam,
so build it once and give it both consumers."*

`BankFromOverlap` sorts pawn from loose cargo, `BankCarriedLoot` / `BankLooseActor` do the two jobs,
`CommitBank` scores, counts, broadcasts and then destroys - in that order. The semantics are lifted
verbatim from `AGSWarren`, which Michael watched working on 2026-08-21: pawn checked first (a courier
is both a pawn and a loose actor, and the loose path would swallow the goblin); `PutDown` never a
bare `Destroy` (it is the only path that unwinds attach, move-speed effect and replicated
`CarriedActor`); attach-parent guard so a courier's cargo is not banked out from under it; broadcast
before destroy so listeners get a live actor.

**DOES NOT OWN AN OVERLAP VOLUME.** The owner decides what "arriving" means and calls in. The Warren
has a banking sphere and the runic site already has an extraction sphere - a third volume would mean
two circles fighting over the same ground on the portal.

**LOOT, NEVER DEEDS, with no flag to change it.** GDD 9: deeds bank only through the real portal at
the END of a raid, because a second place to bank them would erase the reason to risk the run home.

**`Raid/GSRunicSite.{h,cpp}`** - gains the component and calls it.

- **Bank BEFORE extract, and the order is the point.** Once the portal is open `TryExtract` ends the
  raid on contact, so a goblin walking in carrying a pig must have the pig counted before the raid is
  over, or the last armful of the run is silently lost.
- **The extraction sphere now also overlaps `WorldDynamic` and `PhysicsBody`.** It was pawn-only,
  which was right while it existed purely to extract - and is exactly why loot THROWN at the gate
  would have been ignored forever.
- **`TryExtract` is now guarded to pawns.** Before this it received only pawns because nothing else
  could overlap; with loose cargo arriving, that assumption has to be written down rather than
  inherited.

## Evaluate

**NOT COMPILED, NOT RUN.** Nine tickets open, editor up, and **#254 records a compile error already
in the tree** (`GSRaidLibrary.cpp` missing `Engine/OverlapResult.h`, `FOverlapResult` undefined), so
any build dies before reaching this work.

**Verified by evidence:** only that the semantics being copied are the ones Michael watched succeed -
the Warren banked a carried pig for 40 loot and dropped loot from the mouth, and `GS.Warren.Status`
reported `score: loot=40 (source seen: yes)`, which is what proved `AddLoot` finally reaches the score
subsystem. This component is that same logic; it has never itself executed.

**Written and never run - all of it.** Specifically at risk:

- **The widened sphere channels are reasoned, not measured** - the same caveat that was flagged on
  the Warren's banking sphere and turned out fine there. If loot thrown at the gate is ignored, this
  is the first thing to check.
- **Banking while the portal is CLOSED has never been exercised anywhere.** It is the whole point of
  the feature (mid-raid errands) and it is a path `TryExtract` has never shared with anything.
- **Two banks can now both claim the same cargo** if a Warren is planted inside the portal's circle.
  Whichever overlap fires first wins and destroys it, so the outcome is correct but arbitrary. Not
  worth solving until someone plants a Warren on the gate.

**Touched outside the goal:** nothing.

**AGENT_STATE.md owes a line** that GDD 9's "AddLoot has zero callers" defect is now stale twice over
- the Warren closed it and the portal now shares the seam.

## Refine

**Deliberately left undone:**

- **`AGSWarren` is NOT migrated onto this component.** Its banking is verified, watched behaviour;
  re-plumbing it while adding a new consumer would put both at risk in one change. The duplication is
  temporary and is the follow-up, with its own observation requirement. Recorded here so the next
  reader knows it is a decision, not an oversight.
- **A BeginPlay sweep on the portal**, which the Warren has. The Warren's sweep swallowed three items
  on the first frame of a PIE session because the test loot was inside its radius - correct
  behaviour, surprising to watch. The gate sits where the player spawns; a sweep there would eat the
  starting dressing. If designers want it, it should be opt-in.
- **The other three parts of "do it all"** are blocked by other agents' claims, not by effort:
  the T binding needs `GSPlayerCharacter` (#253, claude-acf), horn-summons-from-the-gate needs
  `GSHordeSubsystem` (#239, claude-acf, and **lower-numbered so it has right of way over me**), and a
  build needs the queue empty plus #254's fix. claude-acf confirmed live by Michael 2026-08-22.

> 2026-08-23T00:31Z Portal banking written, uncompiled. T binding and horn-from-gate blocked on claude-acf claims (#253, #239).


### Post-build truth, 2026-08-23

**BUILT (dll 15:38) AND WATCHED.** The "NOT COMPILED, NOT RUN" text above was true when written and
is now stale; this section supersedes it rather than leaving the reader misled.

Michael confirmed: **T works** - hold raises the ghost, green on the arena floor, release plants a
Warren - and **portal banking works**.

**Known open, accepted for now:** the ghost colour still flickers. `GhostVisualLift` (8uu) did not
cure it, which points at the placeholder mesh rather than the offset: the engine `Cylinder` has its
pivot at the CENTRE, so roughly half of it is below ground regardless of the lift. The fix is either
a much larger lift or offsetting by half the mesh height - and it disappears entirely once the mouth
has real art and stops being a cylinder. Michael chose to move on.

> 2026-08-23T22:42Z Built 2026-08-23 15:38 and watched.
