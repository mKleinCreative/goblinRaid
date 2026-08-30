---
id: 346
title: Editor crashes on Play: Knight/Archer/Brawler archetype rows still assign BT_Militia/BT_Archer, whose 5-key BB_Human collides with ACF's cached 13-key blackboard indices
agent: claude-combat
status: done
claimed: 2026-08-29T02:10Z
build: none
waiting_on:
evaluated: 2026-08-29T02:49:47Z
observed: 2026-08-29T02:47:39Z | Editor stopped crashing on Play. Before: assertion 'Array index out of bounds: 11 into an array of size 5' in UBlackboardComponent::GetValue<Float> from AACFAIController::HandlePerceptionUpdated, twice. After: 'PIE: Play in editor total start time 1.551 seconds' with no assertion, and the guard logged REFUSED once per Knight and Archer naming the archetype row.
scenario: PIE on L_Tutorial_Island, the map that crashed on Play twice at 01:58 and 02:07; relaunched after the guard build and pressed Play.
files: 
  - Content/AI/DA_Race_Human.uasset
  - Content/AI/DA_Race_Goblin.uasset
---

## Goal

Editor crashes on Play: Knight/Archer/Brawler archetype rows still assign BT_Militia/BT_Archer, whose 5-key BB_Human collides with ACF's cached 13-key blackboard indices

## Generate

**Diagnosis, from the crash log outward.** The editor died twice on Play (01:58 and 02:07) with:

    Assertion failed: (Index >= 0) & (Index < ArrayNum)
    Array index out of bounds: 11 into an array of size 5
    UBlackboardComponent::GetValue<UBlackboardKeyType_Float>()
    AACFAIController::HandlePerceptionUpdated_Implementation()  [ACFAIController.cpp:169]

`AACFAIController::OnPossess` caches blackboard key **indices** by name against its own tree's
blackboard (`ACFAIController.cpp:88-97`). Measured the two blackboards: `ACFAIBB` has **13** keys and
index **11 is `HomeDistance`, a Float** - matching the callstack type exactly - while `BB_Human` has
**5**. `AGSAIControllerBase::OnPossess` then called `RunBehaviorTree` on the archetype tree,
re-initialising the blackboard component with the smaller asset, so ACF's cached index 11 read off
the end on the first perception stimulus.

**Why the data was wrong again.** Earlier in the day all four `DA_Race_Human` rows were verified
`None`. By the crash, `Archer -> BT_Archer` and `Knight -> BT_Militia` were back, and
`DA_Race_Goblin`'s `Brawler -> BT_Militia` had never been cleared. Peer session goblinsiege-5-8-07
cleared the human rows **in memory only** - never saved - and the crash-and-reload restored the
on-disk values. Same failure mode as `save_asset` silently returning False during PIE.

**No files changed under this ticket.** The rows cannot be cleared from Python:
`FGSArchetypeDefinition::BehaviorTree` is `EditDefaultsOnly`, which the Python API refuses to set on
a DataAsset instance, and a freshly constructed struct is refused the same way. The fix moved into
code - see #347.

## Evaluate

**Observed:** Play went from asserting twice to `PIE: Play in editor total start time 1.551 seconds`
with the bad rows *still assigned*, and the guard naming both offenders in the log.

**Verified rather than assumed:** the blackboard key counts (13 vs 5) and the identity of index 11
were read from the live assets. The Float type in the callstack matches `HomeDistance` and nothing
else at that index, which is what turned a plausible story into a confirmed one.

**Written but never run:** nothing; no asset was modified here.

**AGENT_STATE.md owes:** FAILED - *clearing an archetype BehaviorTree by hand does not hold. It has
now been wrong, cleared, and wrong again inside one day, because an in-memory edit during PIE is not
a saved edit.*

## Refine

**Changed in response to my own evaluation:** the plan was to clear three data fields. When Python
refused, the better answer became obvious - the data will drift again, so the guard belongs in code
(#347). Michael settled it: "this needs to be on tutorial island so we stop wasting time."

**Deliberately left undone:**

1. **The three rows are still wrong on disk** - `DA_Race_Human` Archer/Knight and `DA_Race_Goblin`
   Brawler. Now harmless, but they log an Error per pawn per PIE session. Three clicks in the editor;
   cannot be scripted.
2. **Brawler is untested** - nothing currently spawns one, so the guard effect on it is reasoned,
   not observed.
