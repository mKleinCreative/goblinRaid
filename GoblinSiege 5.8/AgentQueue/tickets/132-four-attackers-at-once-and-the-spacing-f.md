---
id: 132
title: Four attackers at once, and the spacing floor still lets them press in
agent: claude-space2
status: done
claimed: 2026-08-12T00:43Z
build: none
waiting_on:
evaluated: 2026-08-12T01:16:47Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.h
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.h
  - GoblinSiege 5.8/Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSDebugCommands.cpp
---

## Goal

Four attackers at once, and the spacing floor still lets them press in

Michael, 2026-08-11: *"people still are walking a little close together and not backing up. Also,
goblins tend to sit around and not attack, I want to up the limit of attacking goblins at a time up
to 4 so we don't run into a circle stand still."* Then, mid-implementation: *"go ahead and increase
the amount of distance for personal space as well."*

Two complaints that pull in opposite directions - more attackers AND more room - which is why the
spacing had to be paid for out of a budget rather than simply turned up.

## Generate

**The claimed file list grew by four.** `BTTask_MeleeAttack.cpp` and a non-existent
`AI/Services/BTService_CombatStation.cpp` were claimed on a guess before reading; neither was
touched. `BTService_AcquireTarget.*`, `GSAIControllerBase.*`, `GSPlayerCharacter.cpp` and
`GSDebugCommands.cpp` were added. The queue was empty throughout, so nothing was blocked.

### 1. Four attackers - and finding out that the token budget was only half the reason

`TokenBudget` 2 -> 4, `MaxEngagedAttackers` 3 -> 6.

The second number turned out to matter more than the one Michael named. `MaxEngagedAttackers` is
what `UBTService_AcquireTarget::FindNearestHostile` checks through `HasEngagementRoom`, and an agent
refused there **does not get a menace slot - it gets no target at all** and falls to its idle branch.
At 3 assigned / 2 swinging there was exactly one circler per victim and every other goblin was
standing in the corner with `TargetActor` null. That is the literal "sit around and not attack", and
no amount of token budget would have fixed it.

6 is `RingSlotCount`, so every assigned attacker has a place to stand.

**The player is exempt** (Michael's call when asked). `AGSPlayerCharacter`'s constructor calls the new
`ConfigureLimits(2, 3)`. `GSCharacterBase.cpp:29` names *"FOUR defenders deleting the player in a
second"* as the bug the budget of 2 exists to prevent, so shipping exactly four everywhere would have
re-enabled a documented failure in order to fix an unrelated one.

`ConfigureFromArchetype` was renamed `ConfigureLimits` - it had **no callers anywhere in the project**,
so the header defaults were the only numbers this system had ever run on, and "archetype" was a
promise about where the numbers come from that it does not keep.

### 2. Spread out - Michael's follow-up rule

New `CrowdedAttackerThreshold` (2) and `CrowdedDistancePenalty` (0.6) in `FindNearestHostile`: a
candidate already carrying more than 2 attackers *other than the agent asking* scores as though it
were 60% further away per surplus attacker.

A **preference, not a second cap**, and deliberately so: `MaxEngagedAttackers` is already a hard gate
and it has the failure mode hard gates have (see above - refused means no target at all). A scoring
penalty means a crowded victim is a worse deal than a lonely one, so the warband fans out when there
is a choice and still converges when the crowded man is the last one standing.

Counted **excluding self** via the new `GetEngagedCountExcluding` - without that, every incumbent
scores its own target one attacker worse than a stranger would and the warband rotates targets every
scan.

### 3. More personal space, and where the 20uu came from

`PersonalSpaceMargin` 30 -> 50. This could not simply be turned up: the floor must stay **inside**
`RingRadius` or the back-off pushes past the station while the station's spring pulls back in. Worst
pair is guard-to-guard at 138.8uu of capsule, so the ceiling is `RingRadius - 138.8` = **41** at the
old radius of 180.

So `RingRadius` 180 -> 200, paid for exactly:

- `OnStationTolerance` 60 -> 40, for reasons independent of this. Its justifying comment measured
  *"6 slots and OrbitRadius 300, spacing 300uu, so half is 150 and 60 leaves ample room"* - but since
  #108 the station is **not** OrbitRadius 300, it is the ring slot at RingRadius 180 where the spacing
  is 180uu and half is 90. That stale number was slack in two directions: radially "within 60 of a
  point 180 out" means anywhere from 120uu to 240uu, and tangentially two neighbours on adjacent
  slots could each drift 60uu toward each other, so #107's carefully computed 180uu of ring
  separation was really 60uu.
- The freed 20uu goes into the radius. `200 + 40 = 240uu` is the worst legal stand - byte for byte
  what `180 + 60` was. **The attack gate (`AttackRange` 250) sees no change at all**, which is what
  makes this safe against #107's twice-repeated objection that radius is the axis that spends attack
  headroom.

Resulting floors: 188.8 guard->guard, 170.6 guard->player, 154.0 goblin->goblin. Ring spacing
180 -> 200uu. `StandoffRadius` 180 -> 200 to track it.

### 4. "Not backing up" - the back-off was a lean, and arithmetic says so

`BTTask_MenaceOrbit`'s back-off borrowed `StrafeSpeedScale` (0.35) and tapered to zero at the
boundary. At 10uu inside the floor that is gain 0.25 and a movement scale of **0.09** - nine percent
of walk speed, against a crowd pressing in at full speed. #131's own Evaluate computed the
equilibrium at ~2uu *inside* the floor and flagged "whether that reads as a lean or a twitch is
Michael's call". It read as a lean.

- `BackOffSpeedScale` 0.8, its own dial. Sharing the strafe's number was the bug in disguise:
  StrafeSpeedScale is low because a fast orbit "reads as the crowd sliding on ice", but backing out
  of someone's body is the one motion in this node with somewhere it urgently needs to be.
- `BackOffFullDepth` 40 -> 15. 40 was chosen so the single measured 129uu case sat at full push,
  which quietly put every ordinary intrusion far below it.
- `BackOffMinScale` 0.35 - a floor under the taper.
- `BackOffExitMargin` 20 and a `bBackingOff` **latch** in node memory. This is what makes the minimum
  gain safe. #131's taper drove magnitude *and* derivative to zero at the boundary specifically to
  prevent a two-agent limit cycle, and a bare minimum push would have thrown that away. The latch
  replaces the guarantee with a different one: the push stops 20uu *past* the floor, so there is no
  single threshold left for a pair to chatter across. It is also what turns "leaning out of contact"
  into "taking a step back".

### 5. The gap #107 named and left open, which #132 would otherwise have widened

New separation steering on `AGSAIControllerBase::Tick`. #107's own Evaluate:

> *"at peak density (17 live agents) the worst pairwise clearance across ALL agents is still ~0.2uu.
> That number counts bystanders engaged with different victims passing each other, which this
> ticket's ring geometry does not govern."*

Bodies inside each other, by the measurement of the ticket that fixed the ring. **This is almost
certainly the "walking a little close together" Michael is seeing**, and #131's floor cannot reach it:
that floor lives in `MenaceOrbit`, the node an agent runs while it is *not* allowed to attack. At 2
tokens of 3 assigned it covered most of a gang. At **4 of 6 it covers one agent in three** - so
change 1, shipped alone, would have made the crowding worse in the same breath as fixing the
standstill.

Per-pair capsule floor via `GetMinSeparation(Self, Other, 25)`, summed over up to 4 neighbours,
applied as `AddMovementInput`. The agent's **own victim is exempt** - that distance belongs to the
ring and the floor and is the one distance that must stay inside AttackRange.

Not a second position authority (#108): it never writes a goal, it contributes an acceleration that
composes additively with path following in `CalcVelocity`.

**Broadphase sphere overlap at 4Hz, not `TActorIterator`.** `AGSHordeAIController`'s header argues
against ticking in as many words - *"ten goblins ticking to ask the same subsystem the same question
is the cost this design exists to avoid"* - and GDD 3.4 wants the horde perception-less, where "the
forbidden cost is a per-agent SEARCH". Enabling Tick on the base controller puts that cost on every
summoned goblin, so it had better not be a search over the world. The per-frame half is a loop over
at most four cached pointers behind three early-outs.

### 6. Instrumentation

`GS.Combat.Separation 0|1`, mirroring `GS.Combat.PersonalSpace`, and `GS.Combat.CrowdWatch` now
labels samples with **both** switches. Two independent switches shape the numbers that readout
prints - PersonalSpace governs the `victim` bucket, Separation governs `ring` and `cross` - and a
sample labelled with only one of them cannot be reproduced later.

## Evaluate

**BUILD SUCCEEDED in 04:33**, editor closed, `-IgnoreQueue` on Michael's explicit instruction (the
only open ticket was this one). No new warnings - the two C4996 `AbilityTags` deprecations in
`GSGA_Block.cpp` and `GSGA_Interact.cpp` are pre-existing and untouched.

**The one thing I checked in engine source rather than assuming**, because the whole separation steer
is a no-op if it is wrong: does `AddMovementInput` survive while path following owns the velocity?
`CharacterMovementComponent.cpp::CalcVelocity` applies `Velocity += Acceleration * DeltaTime` for
input and then `Velocity += RequestedAcceleration * DeltaTime` for the path follower - **additive,
not exclusive**. A steer from the controller therefore redirects a pathing agent rather than being
swamped by it, and the max-speed clamp afterwards means it cannot make anyone faster. Verified at
`D:\Epic Games\UE_5.8\...\Components\CharacterMovementComponent.cpp`. There was no precedent in this
project to copy: `MenaceOrbit` steers directly, but it runs *instead of* MoveTo, never alongside it.

**NOT VERIFIED - nobody has watched a fight.** Everything else here is design, arithmetic and a
compile. Given `AGENT_STATE`'s FAILED entry (three consecutive fixes shipped unwatched, all three
wrong), that is the whole risk and it should be read as the headline, not a footnote.

**What is arithmetic rather than observation, stated plainly:**

- The "worst legal stand is unchanged at 240uu" claim is exact (`200 + 40` vs `180 + 60`), but it is
  the NOMINAL stand. It says the attack gate's budget is untouched; it does not prove no agent ever
  ends up outside 250 in practice.
- Capsule radii (68.6 / 70.2 guards, 52 goblins) are taken from #107 and #131's measurements, not
  re-measured this session - the VibeUE MCP tools were not available, so no editor query was
  possible. Every floor in the running code is derived from live capsules via `GetMinSeparation`, so
  the CODE is right even if those numbers have drifted; only the margins-vs-ceiling arithmetic in the
  comments would need restating.
- The equilibrium claim for the new latch is qualitative. The latch removes the boundary the old
  taper settled against, so there is no fixed point to compute - the agent should now travel to
  `floor + 20` and stop. Whether that reads as a step or a lurch at `BackOffSpeedScale` 0.8 is
  Michael's eye, and 0.8 is the number most likely to want turning down.

**Coverage, honestly.** Three crowding causes, three owners after this ticket: attacker vs its own
victim = the #131 floor in `MenaceOrbit` (orbiters only, now one agent in three); two agents on one
victim = ring geometry, improved by the tolerance cut but still short of guard-to-guard contact at 6
slots; two agents on different victims = the separation steer, which had **no owner at all** before
today and was measured at 0.2uu clearance in #107. `GS.Combat.CrowdWatch`'s `victim`/`ring`/`cross`
buckets map one-to-one onto those three, which is how the next session finds out which one is still
wrong without guessing.

**Risk I want on the record.** `AGSAIControllerBase` now ticks, and `AGSHordeAIController`'s header
argues against exactly that. The mitigation is real (4Hz broadphase overlap, not a per-frame world
search; three early-outs) but it is untested under a summoned horde. If frame time regresses in a
big raid, `GS.Combat.Separation 0` isolates it in one command.

**Decision line for AGENT_STATE:** the token budget splits by victim from #132 - NPCs 4/6, the player
2/3 (Michael, 2026-08-11). The player is no longer rationed by the same numbers as everyone else, and
`GSCharacterBase.cpp`'s constructor comment asserting that he is has been superseded.

**Verification recipe, one PIE session:**

```
GS.Combat.CrowdWatch 20 4      # baseline, both switches on
GS.Combat.Separation 0         # isolate the new steer
GS.Combat.CrowdWatch 20 4
GS.Combat.PersonalSpace 0      # isolate the #131 floor as well
GS.Combat.CrowdWatch 20 4
GS.Combat.CrowdStats           # caps: expect weight up to 4/4 on a militiaman, 2/2 on the player
```

The `cross` bucket going from negative to positive is this ticket's own claim to have fixed anything.

## Refine

**Changed in response to my own review, before the build:**

- The neighbour scan started as `TActorIterator` and was rewritten as a broadphase sphere overlap.
  Reading `AGSHordeAIController.h` afterwards turned up a direct objection to the design - "ten
  goblins ticking to ask the same subsystem the same question is the cost this design exists to
  avoid" - and GDD 3.4's rule that "the forbidden cost is a per-agent SEARCH". A world-wide actor
  walk on every summoned goblin is precisely that; an indexed overlap at 4Hz is not.
- `TickSeparation` lost its `DeltaSeconds` parameter. It was unused, and the reason it should stay
  unused is worth more than the parameter: the steer is an acceleration the movement component
  integrates itself, so scaling by delta here would make the push weakest at low frame rates - i.e.
  weakest in exactly the pile-up it exists for.
- `AddUnique` rather than `Add` on the neighbour list: an overlap returns one result per COMPONENT,
  so a character can answer with its capsule and again with its mesh, and a neighbour counted twice
  would be pushed against twice as hard as one standing the same distance away.

**Deliberately left undone:**

- **No `.uasset` was edited and no BT node, blackboard key or Blueprint changed**, so `git revert` of
  this commit undoes all of it. That was worth preserving given the size of the change.
- `RingSlotCount` stays 6. Raising it is the obvious way to seat more attackers, and it is wrong:
  `S = 2*R*sin(pi/N)` means 8 slots at radius 200 gives 153uu, which is inside guard-to-guard contact
  again - the exact bug #107 fixed by going the other way.
- `AttackRange` (250) untouched. #131 found the authored sweeps reach 330-390uu, so there is real
  headroom there, but spending it would have made this ticket's spacing changes unfalsifiable: the
  claim "the attack gate sees no change" is only checkable because the gate did not move.
- The two duplicate `HalfHeightOf` lambdas in `GSDebugCommands.cpp` still are not folded into
  `GetBodyRadius`. #131 left them for the one-variable-per-build rule and the same rule applies here.
- No per-archetype budgets. `ConfigureLimits` now has one caller and could trivially take race-data
  rows, but #107 rejected that plumbing as "tuning, not repair" until the positional fix is measured,
  and it still has not been measured.
