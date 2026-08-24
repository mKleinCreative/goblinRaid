---
id: 273
title: 48 ACF errors and warnings a run, of which only one kind was ours - the character data assets had no DefaultAbilitySet
agent: claude-warren
status: done
claimed: 2026-08-24T02:45Z
build: none
waiting_on:
evaluated: 2026-08-24T02:50:44Z
observed: 2026-08-24T02:50:33Z | One clean PIE run with the seven level characters and ten summoned goblins produced zero ACF errors of any severity, where the same run shape had produced twenty-four Invalid Ability Set errors before the change. The seventeen Invalid Character warnings still appear and are ACF logging its own success path.
scenario: L_CombatArena, log length marked then PIE stopped and restarted so only the new lines were counted, goblins summoned with GS.Horde.SpawnTest 4.
files: 
  - Content/Data/Characters/DA_AbilitySet_GS_Empty.uasset
  - Content/Data/Characters/DA_Char_Militia.uasset
  - Content/Data/Characters/DA_Char_Archer.uasset
  - Content/Data/Characters/DA_Char_Knight.uasset
  - Content/Data/Characters/DA_Char_Player.uasset
  - Content/Data/Characters/DA_Char_HordeGoblin.uasset
---

## Goal

Every PIE run logged 48 ACF errors and warnings against characters that appeared to work fine. Find
out which are real and fix those.

**The title this was claimed under said the ability system "fails to initialise on every character".
That was wrong** - it was written from the log before reading ACF's source. Two of the three message
kinds turn out not to be defects at all, and one of them fires *because* the character is healthy.

## Generate

Three distinct messages, and they needed separating before anything could be fixed.

**1. `Invalid Character - ActionsManager` (x24 a run) - NOT A DEFECT. An ACF bug, and it fires on the
HEALTHY path.** `ACFAbilitySystemComponent.cpp:52-59`:

```cpp
StatisticComp = GetOwner()->FindComponentByClass<UACFGASStatisticsComponent>();
if (!StatisticComp) { UE_LOG(ACFLog, Warning, TEXT("No Statistiscs Component - ActionsManager")); }
else                { UE_LOG(ACFLog, Warning, TEXT("Invalid Character - ActionsManager")); }
```

The `else` branch runs when the component **is** found. ACF is logging a scary sentence on success.
Twenty-four of these a run means twenty-four correctly-configured characters. **There is nothing to
fix on our side, and it cannot be fixed on theirs without patching a plugin that lives in the engine
and would be lost on the next ACF update.** Recorded here so the next person who greps the log for
"Invalid" does not spend a session on it - which is most of why this ticket exists.

**2. `Invalid Ability Set` (x24 a run, severity Error) - REAL, AND OURS.**
`ACFCharacterInitializerComponent.cpp:127` calls
`AbilityComp->GrantAbilitySet(CharacterDataAsset->DefaultAbilitySet, FGameplayTag())` **unguarded**,
and `GrantAbilitySet` logs an Error on a null set (`ACFAbilitySystemComponent.cpp:258-259`). ACF's
*other* call site for the same function guards it - `GrantInitialAbilities` at `:65` wraps it in
`if (DefaultAbilitySet)` - so this is an inconsistency inside ACF, but the null is ours: all five
`ACFCharacterDataAsset`s had `DefaultAbilitySet = None`, and the project contained **zero**
`UACFAbilitySet` assets, because abilities are granted through our own GAS path
(`AGSCharacterBase::GrantCombatAbilities`) and not ACF's.

**Fixed** by creating one empty ability set, `/Game/Data/Characters/DA_AbilitySet_GS_Empty`, and
assigning it as `DefaultAbilitySet` on all five character data assets (`DA_Char_Militia`,
`DA_Char_Archer`, `DA_Char_Knight`, `DA_Char_Player`, `DA_Char_HordeGoblin`). Data only - no build.

An empty set is the honest answer rather than a dodge: "this character has no ACF ability set" is
exactly what is true, and null is genuinely invalid input from ACF's side. **It is also the seam the
future ability migration will fill** - when abilities move onto ACF, they go in here.

**HAZARD, for whoever reads this next: `DA_AbilitySet_GS_Empty` is shared by all five characters and
must STAY empty.** Putting an ability in it grants that ability to the player, the horde and every
defender at once. Per-character sets should be new assets, not edits to this one.

**3. `This AACEnemyCharacter should be assigned with a behavior Tree` (x17 a run) - not fixed, on
purpose.** `ACFAIController.cpp:73-74` warns when its own `BehaviorTree` property is unset. Ours is
unset because `AGSAIControllerBase` and `AGSHordeAIController` run their trees themselves; the field
became visible to us when #214 reparented onto `AACFAIController`. Setting it would make ACF run the
tree *as well*, which is a second `RunBehaviorTree` on the same pawn - an AI migration step with its
own risk, not a log-noise fix. Left warning, deliberately, and recorded rather than silently endured.

## Evaluate

**Measured on one clean PIE run** - log length marked, PIE stopped and restarted, then only the new
lines counted - on `L_CombatArena` with the 7 level-placed characters (6 defenders + player) and 10
goblins summoned by `GS.Horde.SpawnTest`:

```
Invalid Ability Set                     0      (was 24)
Invalid Character - ActionsManager      17     ACF's success-path bug, expected
should be assigned with a behavior Tree 16     deliberate, see above
Locomotion State inexistent             17     pre-existing, unrelated - see below
ACFLog: Error  (ALL kinds)              0      (was 24)
```

**Zero ACF errors of any severity now come out of a full run.** That is the whole win: it was the
only Error-level line ACF produced, and it was drowning real ones.

**The measurement needed care and did not work first time.** An earlier count still showed 7 errors
after the fix, which looked like the fix had failed for level-placed actors. It had not - that run
had been started before the asset save propagated. Rather than reason about it, the log was marked,
PIE was cycled, and one clean run was counted from scratch. **The first number was measurement error,
not a finding**, and is recorded because it would otherwise look like a real level-placed/spawned
split to anyone re-reading the log.

**Established:** the three message sources, each read in ACF's own source with line numbers; the
before/after counts on a controlled run.

**Not established:** that an empty ability set is what ACF's authors intend for a character with no
ACF abilities. It satisfies the check and the check is theirs, but their sample content
(`/Game/FullSample/`) is not installed here, so there is no reference character to compare against.

## Refine

**Changed from the plan:** the ticket was claimed against `GSCharacterBase.{h,cpp}` and
`BP_HordeGoblin.uasset` on the assumption this would be a C++ initialisation-order bug like #272.
It was not - reading ACF's source first showed it was five null fields in data. **No C++ was written
and no build was taken.** The claim was corrected to the assets actually touched.

**Not done, and why:**

- The `Invalid Character` bug is ACF's and unfixable from here without patching the engine plugin.
  Documented instead. If ACF is ever forked or a support ticket is raised upstream, this is the line.
- The behaviour-tree warning is a genuine migration decision, not noise. It belongs with whatever
  phase moves the AI onto ACF's controller properly.

**Surfaced, unrelated, not investigated:** `Locomotion State inexistent` fires exactly once per
character, 17 a run, from `LogTemp`. It has been on the backlog as an unexplained hypothesis since
the animation work and is still unexplained. It is now the loudest remaining per-character line in
the log.
