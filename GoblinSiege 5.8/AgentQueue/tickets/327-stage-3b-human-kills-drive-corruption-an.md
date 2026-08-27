---
id: 327
title: Stage 3b - human kills drive corruption, and civilians corrupt more than soldiers (ruling 62)
agent: claude-corruption
status: done
claimed: 2026-08-26T23:21Z
build: required
waiting_on: NOT COMPILED - gate closed by 8 tickets. Then one build closes 325 and 327 together: PIE, kill a few militia and a peasant, and GS.Corruption.Dump should show the kill line split into soldiers and civilians with the meter climbing past 0.80.
evaluated: 2026-08-26T23:24:13Z
observed: 2026-08-27T21:53:21Z | Killed two guards and a peasant and watched the corruption meter climb from 0.02 to 0.06 with no console override: the kill line read 2 soldiers plus 1 civilian at x2.50 for 4.5 weighted, so the civilian was worth two and a half soldiers exactly as ruling 62 asks
scenario: PIE in L_Tutorial_Island after the 14:31 build, killing defenders then GS.Corruption.Dump
files: 
  - Source/GoblinSiege/World/GSCorruptionSubsystem.h
  - Source/GoblinSiege/World/GSCorruptionSubsystem.cpp
---

## Goal

Stage 3b - human kills drive corruption, and civilians corrupt more than soldiers (ruling 62)

## Generate

The last dead driver. Corruption is now reachable at 1.00 from gameplay alone.

**Subscribe** - `OnWorldBeginPlay` binds `AGSGameMode::OnCharacterKilled` (#325) via
`GetAuthGameMode`, which returns null on a client by construction and is why the kill driver is
server-only with no authority check anywhere. `Deinitialize` unbinds; a missing game mode logs loudly
rather than leaving the term silently at zero.

**Classify** - `HandleCharacterKilled` ignores `Race.Goblin`, counts everything else as human
(matching the rule already in the tree at `GSCharacterBase.cpp:258`, one rule not two), and marks a
kill civilian when `AGSEnemyCharacter::GetArchetypeRowName()` equals `CivilianArchetypeRowName`
(Config, default `Civilian`). `DA_Race_Human` carries Militia / Archer / Knight / Civilian.

**Weight (ruling 62)** - `WeightedKills = Soldiers + Civilians x GS.Corruption.CivilianWeight`
(default 2.5), through the same saturating knee as the structures term.

**`SoftKnee` now takes a float**, since kills are weighted rather than counted; the structures call
converts implicitly.

**The dump shows its working**:
`kills 0.20 x 0.31 = 0.06   (4 soldiers + 2 civilians x2.50 = 9.0 weighted, knee 12)`

## Evaluate

**NOT COMPILED.** Gate is closed by eight tickets, most of them other agents'.

**The bug I nearly shipped.** The first version of the civilian branch read
`++(bCivilian ? CiviliansKilled : HumansKilled) - 1;` with a comment calling it a no-op guard. It is
not a no-op: it double-increments `HumansKilled` on every SOLDIER kill, so soldiers would have
counted twice and the civilian weighting would have been measured against an inflated total. I wrote
it, read it back, and it was nonsense on its face - the kind of thing that survives because the
comment beside it sounds confident. `CiviliansKilled` is a strict subset of `HumansKilled` and the
code now says so in a comment placed where the next editor will trip.

**Adversarially:**

- **The knee is still unsized and I have said so in the header.** 12 was drafted against ruling 19's
  15-defender pool; the 2026-08-23 roster ruling made castle guards Militia with "a decent amount of
  them". I tried counting from the .umap files and got reference counts, not instances. Guessing a
  second number off a number I know is unreliable is worse than leaving it visible - so the dump
  prints soldiers, civilians, the weighted total and the knee, and it gets set from a watched raid.
- **2.5 is equally unfounded.** Ruling 62 says "more"; it does not say how much more. If slaughtering
  peasants becomes the efficient route to a black sky, this is the number that did it.
- **`Cast<AGSEnemyCharacter>` is the single point of failure for the whole civilian rule.** Anything
  human that is not an `AGSEnemyCharacter` - a future civilian on its own class, a landlord actor -
  counts as a soldier silently. Not currently reported.
- **`GetRaceTag()` on a corpse is still assumed stable** under ACF's death path (carried over
  from #325, still unverified).
- **Nothing has run.** #325's delegate has still never been seen to fire; this ticket is what makes
  that observable, and neither can be closed until one build and one fight happen.

**Owes `AGENT_STATE.md`:** a DECISIONS line - *"kill term is weighted, not counted; civilians x2.5 by
ruling 62, and the knee is deliberately unsized until a raid is watched"*.

## Refine

**Changed from my own review:** the broken increment above, and the header block that still announced
the kill term as unwired - a file whose own documentation lies about what it does is worse than one
with no documentation.

**Deliberately left undone:**

- **Sizing the knee** and **the civilian multiplier** - both want a watched raid, not another guess.
- **Reporting a failed `Cast<AGSEnemyCharacter>`** - third bullet above. It is a real silent-failure
  hole of exactly the kind this feature keeps finding, and it deserves its own small ticket rather
  than being smuggled in here.
- **Moving the discriminator onto `FGSArchetypeDefinition::RoleTag`** - the cleaner shape, blocked on
  a `DA_Race_Human` edit that needs an editor session.
