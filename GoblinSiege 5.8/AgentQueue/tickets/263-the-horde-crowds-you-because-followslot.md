---
id: 263
title: The horde crowds you because FollowSlot drives nothing: give each goblin a formation post behind the summoner
agent: claude-acf
status: done
claimed: 2026-08-23T22:12Z
build: done
waiting_on: BUILT. Blow the horn and stand still - the band should settle into ranks of three behind you instead of crowding. Judge whether 220uu reads as the right distance.
evaluated: 2026-08-23T22:21:00Z
observed: 2026-08-23T23:22:16Z | Michael played with a summoned band following him and reported that the goblins stopped crowding him - they hold formation posts behind the summoner instead of converging on his position, which is the behaviour this ticket set out to produce.
scenario: PIE in L_CombatArena on the player pawn with a horn-summoned band, 2026-08-23, editor build of 16:09.
files: 
  - Source/GoblinSiege/Horde/GSHordeAIController.h
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
  - Content/AI/BB_HordeGoblin.uasset
  - Content/AI/BT_HordeGoblin.uasset
---
## Goal

Michael: *"you still get crowded by your horde, they don't stay in formation"* - and originally,
*"it would be preferrable if they gather slightly behind you, indicating you're their leader."*

## Generate

**The cause, and it reframes #239.** `Follow Summoner` is a stock `BTTask_MoveTo` pointed at the
`FollowTarget` **object**, which `GSHordeAIController` sets to the summoner's pawn. So every goblin
in the band paths to the same point - the player. `FollowSlot` has been computed and published to the
blackboard since the horde was written and **consumed by nothing**. The controller's own comment says
the ring maths "belongs in the BT's move task"; no such task was ever written. #239 made the slots
stable, which was correct, but the slots were never driving position.

- `UGSHordeSubsystem::GetFollowPostFor` - ranks of three filling backwards from the summoner's heel:
  220 uu to the first rank, 140 per rank behind that, 115 between files. Measured from the summoner's
  ACTOR forward, not its control rotation, so the formation does not swing around every time the
  player looks sideways. Deterministic, because an earlier version rolled `FMath::FRand()` per repath
  and a goblin standing still kept changing its mind about where to stand.
- `BB_HordeGoblin` gains a Vector key `FollowLocation`; `Follow Summoner` now moves to it.
  `FollowTarget` is untouched and still gates WHETHER to follow - the decorator and the
  standing-order suppression both keep working unchanged.
- The controller publishes the goblin's OWN location when it is already within
  `FollowPostTolerance` (130 uu) of its post. `BTTask_MoveTo` snapshots its goal at ExecuteTask and
  ignores later writes - `bObserveBlackboardValue` is hard-false in UE 5.8 - and re-executes
  constantly, so writing a live post every tick would have the whole band micro-stepping forever.
  Same deadband, same reason, as the archers in #247.

## Evaluate

Built (00:55, then 00:14 with the debug commands). The asset edits were verified from a **fresh
editor process** - the BB key and the retargeted move key both read back correctly off disk rather
than from the copy I had just written, which is what `never-edit-bt-bb-assets-via-python` asks for.
The blackboard was edited as an asset, not through the BT editor's tabs.

**Not watched.** No PIE run. The specific things I cannot judge from here: whether 220 uu reads as
"behind me" or "too far back", and whether ranks of three suit a ten-goblin band or want to be four.
All four numbers are plain members at the top of the function.

Untouched deliberately: goblins under a standing order still publish no follow post at all, so an
ordered band behaves exactly as before.

## Refine

Nothing changed on review. Worth recording plainly: #239 was a real fix to a real defect that could
not have been visible, because the value it corrected was never read by anything.
