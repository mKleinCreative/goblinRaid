---
id: 107
title: Crowd fix: orbit on your own slot bearing, 6 slots, honest arrival tolerance, player avoidance
agent: claude-gobkit
status: done
claimed: 2026-08-09T20:56Z
build: none
waiting_on:
evaluated: 2026-08-09T23:29:36Z
files: 
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.h
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - Source/GoblinSiege/Combat/GSEngagementComponent.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Crowd fix: orbit on your own slot bearing, 6 slots, honest arrival tolerance, player avoidance

## Generate

Design came from a four-area workflow with an adversarial reviewer per area. **Two of its eight agents
died on API errors - the BT-ordering design and the final synthesis - so the synthesis below is mine,
assembled from the three surviving designs and their three reviews.** The reviewers' corrections are
applied; where they disagreed I picked and said why.

**1. THE ONE THAT MATTERS: each orbiter steers to its OWN bearing.**
`BTTask_MenaceOrbit::TickTask` free-strafed around a shared `OrbitRadius` circle with no per-agent
phase, so every waiting attacker converged on one arc. It now calls `ClaimRingSlot` and stands at
`OrbitRadius` along **that slot's bearing**. The claim was already exclusive; the orbit simply never
asked which slot it held. At 6 slots that is `2*300*sin(30deg)` = **300uu of guaranteed separation**
against the zero it had. New knobs `OnStationTolerance` 60 and `OnStationShuffleScale` 0.25 - on
station it keeps shuffling, because a motionless waiting attacker is the failure the research names.

**2. `RingSlotCount` 8 -> 6, `RingRadius` stays 180.** Spacing `2*R*sin(pi/N)`: 8@180 = 137.8uu against
a 68.6+70.2 = **138.8uu** pair of guard capsules - adjacent slots were interpenetrating before anyone
moved, so RVO and not the ring decided where people stood. 6@180 = 180uu, clearing the guard pair by
41uu and RVO's 1.2 radius expansion (168.5uu) by 11.5uu. **Clearance bought from slot COUNT, not
radius** - radius is the axis that spends AttackRange headroom, and S responds harder to N.

**3. Arrival tolerance, asset-only.** `BT_Militia` and `BT_HordeGoblin` MoveTo: `AcceptableRadius`
60/50 -> **45**, and `bReachTestIncludesAgentRadius` **off**. That flag was adding a whole capsule, so
militia effective tolerance was 60+68.6 = 128.6uu against 137.8uu spacing.

**4. Slot claims stop expiring under a standing agent.** `ClaimRingSlot`'s early return re-stamps
`ClaimedTime` while the claimant is arrived or still closing, via a new `FGSRingSlot::ClosestApproach`
plus `SlotArrivedRadius` 70 / `SlotProgressEpsilon` 25. Not an unconditional re-stamp - that would make
the watchdog unreachable. `SlotClaimTimeoutSeconds` 4 -> 6.

**5. The player is published to the avoidance manager** (`bUseRVOAvoidance` true, `AvoidanceWeight`
1.0). He was invisible to it, so NPCs discovered him by walking into his capsule. Full weight means
others yield to him and he never yields, so his own movement is untouched.

Also: `GSIsDeadOrGone` in the fresh-claim path, so a corpse cannot hold a ring slot (companion to #106).

## Evaluate

**The reviewers' single most valuable finding, which I have honoured:** all three independently said
the geometry changes are *unobservable on their own*, because MenaceOrbit outranks MoveTo inside 600uu
so the claimed slot is never walked to at contact distance. One put it bluntly - the asset-only change
"can be tested before the C++ batch lands" would have shown **nothing**. That is why change 1 is the
headline and why I did **not** ship the asset edits as a standalone quick win.

**Reviewer disagreement I had to settle:** slot count 5 (spacing 211.6uu) vs 6 (180uu). Took **6**,
because that reviewer checked it against RVO's radius expansion (168.5uu) rather than bare capsule
contact, which is the constraint that actually decides whether bodies shove. 5 is also defensible; 6
keeps more slots than the cap of 3 needs, which is what absorbs churn.

**Asset changes verified against reloaded assets**, not the in-memory objects I wrote - the #086
lesson:

```
BT_Militia       Move To                acceptable=45 agentR=False goalR=False
BT_HordeGoblin   Move to stand-off slot acceptable=45 agentR=False goalR=False
BT_Archer        Hold bow range         acceptable=80 agentR=False goalR=False
BT_HordeGoblin   Follow the summoner    acceptable=250 agentR=True  goalR=True   (deliberately untouched)
```

**BUILT AND MEASURED.** `BUILD SUCCEEDED in 00:52`, no new warnings. Patrol run re-measured.

**The mechanism is confirmed live** - on every victim, engaged count now equals claimed slots exactly
(3/3, 2/2, 2/2, 1/1), which is the new orbit claiming a slot it never used to ask for. And the
distances show the intended TWO BANDS rather than one shared circle: a slot holder at ~198uu (the
180 ring) and an orbiter at ~322uu (the 300 station).

**Capsule clearance between co-attackers went from interpenetrating to positive:**

```
baseline: min clearance -29.1uu, later 1.1uu     (bodies inside each other)
post-fix: 22.5uu, 93.5uu, 683.9uu, and 78.4uu on a separate run
```

**A CORRECTION TO MY OWN METRIC, which changes how the arc numbers should be read.** I reported
"largest empty arc vs 360/k" as the crowding measure. That is only meaningful when the ring is FULL.
With 6 slots and 2 attackers, the correct spacing is 60 degrees apart on adjacent slots, giving a
300-degree gap - a ratio of ~2.0 that looks like failure and is actually correct behaviour. So the
post-fix ratios of 1.5-2.0x on sparse rings are not a shortfall, and I should not have presented the
ratio as the headline. **The metric that survives scrutiny is capsule clearance between agents engaged
with the SAME victim**, which is unambiguous and moved the right way.

**Honest limit: at peak density (17 live agents) the worst pairwise clearance across ALL agents is
still ~0.2uu.** That number counts bystanders engaged with different victims passing each other, which
this ticket's ring geometry does not govern. Whether that reads as crowding to Michael is a question
for his eye, not for my arithmetic - and it is the reason the separation-steering work from the
workflow's Area 3 is still on the table.

**Deliberately NOT done from the workflow's output:** the per-archetype `ConfigureFromArchetype`
plumbing (four new fields on `FGSArchetypeDefinition`, per-row budgets). Its reviewer returned FLAWED
on the numbers and it is a data-model change that cannot be justified until the positional fix is
measured - if the crowd reads correctly at a uniform 2/3, per-archetype budgets are tuning, not repair.

## Refine

**Rejected the workflow's ring-radius derivation** (radius from victim capsule + reach, 180 -> 186 with
two new UPROPERTYs). Its own author showed every capsule in the project lands on the floor value, so it
changes nothing today, and both reviewers said radius is the axis that costs AttackRange headroom. It
buys nothing until there is an ogre.

**Kept the archer's tolerance loose (80, flags off).** Ground truth says the archer already works;
tightening it to melee numbers would have been consistency for its own sake against a working system.

**Left undone:** teaching `GS.Combat.CrowdStats` to report largest-empty-arc and minimum capsule
clearance, so the readout can see the complaint without me writing Python each time.
