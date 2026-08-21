---
id: 225
title: ACF Phase 2c: audit the class-identity checks against the AACFCharacter reparent
agent: claude-acf
status: done
claimed: 2026-08-21T01:30Z
build: none
waiting_on:
evaluated: 2026-08-21T01:31:47Z
observed: UNOBSERVED 2026-08-21T01:31:47Z - Read-only audit that changed no code - there is nothing to watch. Its findings are cited to file:line in ACF source and to this session log.
scenario: none - never run
files: 
  - Source/GoblinSiege/AI/GSBuffAuraComponent.cpp
  - Source/GoblinSiege/World/GSFireVolume.cpp
---

## Goal

ACF Phase 2c: audit the class-identity checks against the AACFCharacter reparent

## Generate

**No code changed. The audit's answer is that the hazard does not apply, and the real consequence of
the reparent is somewhere else entirely.**

### A. The eight class-identity checks are unaffected

Every cast on our character classes, found by grep rather than from the AGENT_STATE list:

`GSAIControllerBase.cpp:98`, `GSBuffAuraComponent.cpp:48`, `BTTask_DeliverCargo.cpp:46`,
`BTTask_MeleeAttack.cpp:261`, `GSDamageExecCalculation.cpp:188`, `GSHordeAIController.cpp:53` and
`:94`, `GSGA_DodgeRoll.cpp:78`.

The chain after Phase 2a:

```
AACFCharacter  <-  AGSCharacterBase  <-  { AGSEnemyCharacter, AGSPlayerCharacter, AGSHordeGoblin }
```

The reparent inserted a class **above** `AGSCharacterBase`, so no relationship *among* our classes
moved: a goblin is still not an `AGSEnemyCharacter`, a defender is still not a player. **Inserting a
base cannot change a downcast between siblings.** The migration doc's warning was inherited from a
different move - reparenting `AGSHordeGoblin` onto `AGSEnemyCharacter` - which would have changed
these and is still not being done.

### B. What DID change: ACF's AI machinery woke up

`AACFAIController::OnPossess` (`ACFAIController.cpp:63-74`):

```cpp
CharacterOwned = Cast<AACFCharacter>(possPawn);
if (!CharacterOwned) { ...early return... }     // line 65
...
if (!BehaviorTree) { UE_LOG(... "should be assigned with a behavior Tree" ...) }   // line 73
```

Before Phase 2a our pawns were not `AACFCharacter`, so **line 65 returned and everything below it was
dead code**. Phase 1's header comment predicted exactly this and it has now happened: our pawns pass
the cast, the early return no longer fires, and ACF's `OnPossess` runs to the end - `CharacterOwned`
is valid, its blackboard init runs, and it reaches the tree check. That is the source of the six
`This AACEnemyCharacter should be assigned with a behavior Tree` warnings, one per defender.

**ACF's `StartTree()` does not run only because no ACF `BehaviorTree` is assigned.** Our trees still
come from our own `RunBehaviorTree()` calls after `Super`, which remain load-bearing.

### C. The consequence that matters, for ruling 33

ACF's attacker ticketing (`UACFAIManagerComponent`, **`MaxAttackersPerTarget = 1`**) is consumed only
by `UACFCombatBehaviourComponent::TryExecuteConditionAction` (`ACFCombatBehaviourComponent.cpp:146`),
which is driven by ACF's behavior tree. Ruling 33 established that this was inert. **It still is -
but the reason has changed.** It used to be unreachable because `CharacterOwned` was null; now it is
unreachable only because nobody has assigned an ACF BehaviorTree. It is one asset assignment away
from clamping the whole horde to one attacker per target.

**Ruling 33's insurance half - "set `MaxAttackersPerTarget` above 1 regardless" - was recorded and
NEVER IMPLEMENTED.** It needs a placed `UACFAIManagerComponent` to set it on, which this project does
not have. That is now a live debt rather than a precaution.

### D. `ACFLog: Warning: Invalid Character - ActionsManager` is an ACF logging bug

`ACFAbilitySystemComponent.cpp:52-59`:

```cpp
StatisticComp = GetOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
if (!StatisticComp) { UE_LOG(... "No Statistiscs Component - ActionsManager") }
else               { UE_LOG(... "Invalid Character - ActionsManager") }
```

The message fires in the **else** branch - when the statistics component **is** found. It reports
success in the words of a failure. Harmless noise; do not chase it.

## Evaluate

**Nothing was built or run for this ticket - it changed no code.** Every claim above is read off ACF
source and off this session's log, and each carries a file:line so the next reader can check rather
than trust.

**What this ticket does NOT do:** clear ruling 33's debt (C), or verify Phase 2a's sprint fix, which
shipped after the only fight anyone has watched.

## Refine

Closed as a no-op deliberately rather than inventing a change to justify the ticket. The audit's
value is B, C and D - none of which were what it was opened to look for.
