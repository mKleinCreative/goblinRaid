---
id: 189
title: Rename the granary objective: AGSObjective_BurnGranaries -> AGSObjective_ToppleStatue, Objective.Granary -> Objective.Statue, with CoreRedirects - WRITTEN, needs build
agent: claude-statuerename
status: review
claimed: 2026-08-18T22:12Z
build: required
waiting_on:
evaluated: 2026-08-18T22:16:17Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Missions/GSObjective_BurnGranaries.h
  - Source/GoblinSiege/Missions/GSObjective_BurnGranaries.cpp
  - Source/GoblinSiege/Destruction/GSDestructibleObjective.h
  - Source/GoblinSiege/Destruction/GSDestructibleObjective.cpp
  - Config/DefaultEngine.ini
---

## Goal

Rename the granary objective: AGSObjective_BurnGranaries -> AGSObjective_ToppleStatue, Objective.Granary -> Objective.Statue, with CoreRedirects - WRITTEN, needs build

## Goal (context)

Flagged as the out-of-scope half of #179 and asked for directly by Michael. #156 removed the
granary from the GDD on 2026-08-14; the Statue took its place in the required trio and lands on
`AGSDestructibleObjective` (a GeometryCollection released on destruction - what toppling stone
wants and what burning never did). The mission class that counts them was still called
`AGSObjective_BurnGranaries` and matched the actor tag `Objective.Granary`.

**Scope: names only.** Behaviour is deliberately untouched - see the WARNING below.

## Generate

- `git mv` of `Missions/GSObjective_BurnGranaries.{h,cpp}` -> `GSObjective_ToppleStatue.{h,cpp}`
  (git mv, so the rename is visible as a rename rather than a delete + add).
- Class `AGSObjective_BurnGranaries` -> `AGSObjective_ToppleStatue`; members
  `GranaryActorTag` -> `StatueActorTag`, `HandleGranaryDestroyed` -> `HandleStatueDestroyed`,
  `TotalGranaries`/`BurnedGranaries` -> `TotalStatues`/`ToppledStatues`.
- `Destruction/GSDestructibleObjective.cpp`: the constructor actor tag
  `Objective.Granary` -> `Objective.Statue`. Producer and consumer changed together, so the two
  sides still agree - verified by grep, three hits, all `Objective.Statue`.
- `Config/DefaultEngine.ini`: new `[CoreRedirects]` section with a `+ClassRedirects` entry from
  `/Script/GoblinSiege.GSObjective_BurnGranaries` to the new name.
- Granary-era comments corrected in the files above.

## Evaluate

**NOT COMPILED. NOT OPENED IN THE EDITOR. Do not close this on the diff.** The build gate was
shut throughout (8+ tickets), so `Build-GoblinSiege.ps1` never ran.

What was checked, and how:

- **No C++ caller anywhere.** `grep -rn "BurnGranaries" Source/` before the change returned only
  the class's own two files plus three prose comments. Nothing constructs, casts to, or includes
  it. The rename cannot break a compile unit other than its own.
- **Producer/consumer agreement on the new tag** - grep returns exactly three `Objective.Statue`
  hits: the `Tags.Add` in the constructor, the `StatueActorTag` default, and one comment.
- **No granary left in the files touched** except two deliberate rename-history notes.

**What I could NOT verify, and why it matters.** The editor is running but its Python bridge
timed out twice on asset-registry queries (other agents hold editor tickets), so I could not
enumerate Blueprint subclasses or placed instances. Two distinct risks, and the mitigation only
covers one:

- **Class references ARE covered.** A Blueprint deriving from the old class, or a level with one
  placed, resolves through the CoreRedirect. This is the standard UE mechanism for exactly this
  and is why the rename is safe without the enumeration.
- **A hand-typed actor tag is NOT covered.** CoreRedirects remap classes, not `FName` tags in an
  actor's Tags array. If a designer manually typed `Objective.Granary` onto an actor in
  `L_Tutorial_Island` rather than inheriting it from the CDO, that actor silently stops being
  counted. Nothing warns. **This is the one thing to look for when the editor is free.**

## Refine

**A finding bigger than the rename, recorded and deliberately not acted on.**
`AGSDestructibleObjective` gates its Chaos fracture behind `UGSFlammableComponent::OnBurnedDown`
and owns a `UGSFlammableComponent` outright - so as written, the statue **completes by burning**.
GDD 2.8: *"The statue is the one target that doesn't burn: it has to be brought down, stone on
stone."* The class is right for the statue (geometry-collection release) and its trigger is
wrong.

That is a behaviour change, not a rename, and it is the kind that has to be watched in PIE rather
than reasoned about. Left as a loud WARNING block at the top of `GSDestructibleObjective.h`
pointing at this ticket. **Wants its own ticket, after a build.**

Also left undone: "granary" survives in ~12 unrelated comments across the Destruction module as a
generic example of a hypothetical fourth burn type ("a granary, a tannery"). Those read fine as
hypotheticals and are in files this ticket does not hold.
