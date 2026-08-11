---
id: 131
title: Personal space: a capsule-derived minimum-distance floor in the orbit, plus the spatial instrumentation nobody had
agent: claude-space
status: done
claimed: 2026-08-11T17:08Z
build: none
waiting_on:
evaluated: 2026-08-11T17:15:12Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.h
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

Personal space: a capsule-derived minimum-distance floor in the orbit, plus the spatial instrumentation nobody had

## Generate

Michael: *"Enemies shouldn't be pressing up so close to each other. we need to get them to try and
keep a minimum distance... Maybe use a volume that causes them to back up if they get in your
personal space."* Designed by a research workflow (11 agents, plan at
`scratchpad/spacing_plan.md`); this ticket implements it.

**The ring was never the bug.** The station is 180uu and guard-to-guard capsules need 138.8, so the
nominal geometry has 41uu of daylight. What failed is that the on-station test at
`BTTask_MenaceOrbit.cpp:199` is **unsigned**: `StationError > OnStationTolerance` with a tolerance of
60 means "on station" is satisfied anywhere from **120uu to 240uu**, and 120 is inside capsule contact
for every pair in the game (guard-to-player touch at 120.6). They were not overshooting a good
target; they were told a bad one was acceptable. Measured live: closest pair 129uu against 138.8
needed, others at 140/149/178.

**Step 0 (plan's blocking read) came back clean.** `LungeSpeed = 0.0` on every stage of all four
sword abilities, so `LaunchCharacter` is never firing and the lunge is not the root cause. The
authored sweeps are also bigger than the C++ defaults the plan assumed - forward offset 95, radius
140/150/165 by stage - giving real reach of 330-390uu against a 168.8 floor. That is enormous
headroom, so the main risk of a minimum distance ("they stand apart and never swing") is far smaller
than the plan feared.

**What was built:**

1. `UGSEngagementComponent::GetBodyRadius()` / `GetMinSeparation()` - the body-size accessor this
   module did not have. Every capsule read in the project was inline: two byte-identical
   `HalfHeightOf` lambdas in this very file, a hardcoded 42 in `GSRaidLibrary`. Scaled radius, so a
   Blueprint-scaled character is measured as it actually is.
2. `GetEngagedActors()` / `GetSlotClaimant()` - `Engaged` and `RingSlots` were private with only
   count accessors, which is why four crowding tickets could report "the caps hold" while the crowd
   looked wrong.
3. **The floor**, in `BTTask_MenaceOrbit::TickTask`: a back-off branch before the station seek, firing
   when `Distance < GetMinSeparation(Self, Target, PersonalSpaceMargin)`, pushing radially outward
   with a gain tapered by depth. Three new dials: `PersonalSpaceMargin` 30, `FeintMargin` 0,
   `BackOffFullDepth` 40. The feint keeps its own floor at capsule contact so it can still close.
4. `Dist2D` in `BTTask_MeleeAttack` at both the attack gate and the guard-break gate. A 3D distance
   spent the 57uu height difference between a guard and a goblin out of the same 250uu budget,
   leaving 243.3uu of real horizontal reach against a 240uu worst legal stand - 3.3uu of margin,
   which is why "stands there and never swings" was intermittent. Values unchanged; this only ever
   makes swinging more likely, which is what makes the floor safe to ship.
5. `[GS.Space]` in `GS.Combat.CrowdStats`, `GS.Combat.CrowdWatch <sec> [hz]`, and
   `GS.Combat.PersonalSpace 0|1`.

**Why a distance test and not the trigger volume Michael suggested:** the radius is a per-PAIR
quantity `f(rA, rB)` - a sphere on a guard cannot know whether the body inside it is a 52uu goblin or
a 70uu guard, so one bubble would have to be sized for the worst case and over-space everything else.
Overlap events are a step function, and a boolean shove at a threshold is the standard way to build a
two-agent limit cycle. And `MenaceOrbit` already computes the outward direction and already steers
with it, so this is one `else if` in existing code rather than a second authority over position -
which is precisely the regression #108 exists to prevent. Same idea, one layer lower.

## Evaluate

**Verified:** build result in Refine. The plan's central claim was checked against source before
implementing - `OnStationTolerance = 60` at `BTTask_MenaceOrbit.h:80` and the unsigned comparison at
`.cpp:199` are both exactly as described.

**Equilibrium, checked on paper:** the shuffle's inward spring at `StationError = 11.2` is
`0.35 x 0.25 x (11.2/60)` = 0.016; the outward taper matches at `Gain = 0.047`, i.e. depth ~1.9uu. So
an undisturbed agent settles about 2uu inside the floor rather than chattering. Whether ~2uu reads as
a lean or a twitch is Michael's call, not arithmetic's.

**NOT verified - nobody has watched a fight.** Everything above is design and compilation. Given this
project's record (`AGENT_STATE` FAILED: three consecutive fixes shipped unwatched, all three wrong),
that is the whole risk. The A/B cvar exists specifically so the comparison happens inside one PIE
session instead of against a memory.

**Coverage, stated honestly.** The floor governs attacker <-> its own victim, which includes every
"guard pressing into the player" case because the player carries the same engagement component. It
does NOT govern two attackers on one victim (the ring's 180 does, imperfectly) and does NOT govern
two agents attacking different victims (nothing does; #107 left that on the record). The `role`
column exists to tell us which of the three produced 129 - and if it says `ring` or `cross`, this
floor is the wrong fix and the plan's fallbacks apply.

**Also unverified:** that #130's mobility restore did not already change the numbers. Michael watched
a duel after it and reported no visible change, but nobody re-measured. The baseline run with
`PersonalSpace 0` settles it.

## Refine

**Changed while implementing:** the plan had `CrowdWatch` reading the personal-space cvar directly,
which is a file-static in another translation unit. Read through `IConsoleManager` instead of
exporting a symbol - the readout only needs to LABEL which side of the A/B a sample came from, and a
missing cvar degrades to "OFF" rather than failing.

**Deliberately left undone:** the two duplicate `HalfHeightOf` lambdas in this file were NOT folded
into the new `GetBodyRadius`, per the plan's one-variable-per-build rule. `RingRadius` was not
touched (#107 rejected growing it), `RingSlotCount` stays at 6 (#107 chose it deliberately), no
`.uasset` was edited, and no BT node or blackboard key changed - so `git revert` undoes all of this
completely, which after tonight's asset trouble was worth preserving.
