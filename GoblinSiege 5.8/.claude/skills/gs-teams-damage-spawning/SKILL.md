---
name: gs-teams-damage-spawning
description: Goblin Siege — ACF's group spawner that never spawns, the damage handler that crashes on a structure, the team attitude ACF never reads from your data asset, and the revive half that was never migrated.
globs: []
alwaysApply: false
---

# GS — spawning, damage and teams: what adopting ACF here actually costs

Six traps around `AACFAIGroupSpawner`, `UACFDamageHandlerComponent`, the team manager and revive. Three
of them are reasons to adopt ACF anyway; three are reasons not to, or not yet. All of them are silent or
present as something else. Read before adding an ACF spawner, bolting a damage handler onto a structure,
editing `DA_GSTeams`, or writing a "bring this character back" verb.

---

## 1 — A level-placed `AACFAIGroupSpawner` never spawns

```
// ACFAIGroupSpawner.cpp:44-48
if (AIGroupComponent && GetOwner() && GetOwner()->HasAuthority()) {
    if (bSpawnOnBeginPlay) { AIGroupComponent->SpawnGroup(); }
}
```

**A level-placed actor has no Owner**, so `GetOwner()` is null and the whole block is skipped — silently,
no warning. The intended authority test is `HasAuthority()` on the actor itself; the group component
does its own correct check at `ACFGroupAIComponent.cpp:89`. `AACFVolumeAIGroupSpawner` is unaffected
because it spawns from the overlap handler instead (`ACFVolumeAIGroupSpawner.cpp:50-58`, `75-78`).

`ai-framework:172` prescribes exactly the sequence that fails: place one, tick `bSpawnOnBeginPlay`. The
first person told to "just use ACF's group spawner instead of `GSSpawnerActor`" sees an empty street and
nothing in the log, concludes the spawner is broken, and writes a third bespoke spawner.

**Adoption requires** calling `AIGroupComponent->SpawnGroup()` yourself (GameMode, raid director, or a
volume), or subclassing and overriding BeginPlay.

---

## 2 — Adopt the group component for the cap and the navmesh projection

`AGSSpawnerActor` is nine lines and looks innocent: `StartSpawnTimer` sets a **looping** timer at 9s
base, tightened 10% per reinforcement tier to a 1.5s floor (`AI/GSSpawnerActor.cpp:73-85`), and
`SpawnUnit` spawns at `GetActorLocation(), GetActorRotation()` with **no cap, no offset, no navmesh
query and no roster** (`:87-107`). Nothing counts live defenders; the only stops are `Silence()` from
structural damage or burn-down (`:109-126`) and the district-razed flag (`:163-173`).

The raid clock is **1800 seconds** (`Core/GSGameState.h:251`). One barracks at 9s and never silenced is
~200 defenders; at Tier-4 pacing closer to 400 — **per building**, all at the identical world transform
inside the building mesh. Symptom: late-raid framerate collapse and guards jammed in a doorway, which
reads as a navigation or animation problem and gets investigated there.

What `UACFGroupAIComponent` gives for free:

| Feature | Source |
|---|---|
| refuses past `MaxSimultaneousAgents` (default **20**) | `ACFGroupAIComponent.cpp:365`; `ACFGroupAIComponent.h:60-61` |
| jitters each spawn by `DefaultSpawnOffset`, default **(150,150)** | `cpp:39`, `:382-384` |
| `K2_ProjectPointToNavigation` with a **500uu** search extent before spawning | `cpp:388-394` |
| deferred spawn with `AdjustIfPossibleButAlwaysSpawn` | `cpp:396-406` |
| assigns lead actor, default AI state, patrol path; initialises from a `UACFCharacterDataAsset` at a level | `cpp:181-199` |
| removes the dead from the roster, fires `OnAllAgentDeath` | `cpp:637-647` — which `UACFAIWavesMasterComponent` chains waves off (`ACFAIWavesMasterComponent.cpp:43-72`) |

The cap is the single most valuable thing here; the navmesh projection is second, and it is also what
would stop a newly-spawned guard appearing off-mesh.

**NOT VERIFIED and cannot be:** FullExample has no group spawner, no waves component and no populated
island — any claim about "how the sample populates its island" would be invention.

---

## 3 — `SetGroupOwner` discards `bOverrideAgentTeam` and null-derefs perception

```
// ACFAIController.cpp:607-620
void AACFAIController::SetGroupOwner(UACFGroupAIComponent* group, int32 groupIndex, bool disablePerception, bool bOverrideTeam)
{
    if (group) {
        GroupOwner = group;  GroupIndex = groupIndex;
        UAIPerceptionComponent* perception = FindComponentByClass<UAIPerceptionComponent>();
        if (perception && disablePerception) { perception->UnregisterComponent(); }
        else { perception->RegisterComponent(); }
    }
}
```

Two defects, both invisible to a "read the header" adoption:

1. **`bOverrideTeam` is accepted and never read** — the body has no team code at all — despite
   `UACFGroupAIComponent` exposing `bOverrideAgentTeam = true` as an editable promise
   (`ACFGroupAIComponent.h:86-87`) and passing it on every agent init (`ACFGroupAIComponent.cpp:193`).
   A mixed-team group looks configured and is not.
2. **The `else` branch dereferences `perception` on the path where it is null.** If the controller has
   no `UAIPerceptionComponent`, both branches crash.

Why #2 is a time bomb here: `AGSHordeAIController` deliberately refuses the component —
`: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent")))`
(`Horde/GSHordeAIController.cpp:17`), per GDD §3.4. `GSAIControllerBase.h:58-62` records that the opt-out
currently does **not** work in 5.8 (the engine logs *"Ignored DoNotCreateDefaultSubobject for
AIPerceptionComponent as it's marked as required"*) and that the fix is left "with whoever measures the
cost". The day someone makes it take effect — an engine upgrade, or that ticket landing — every horde
goblin ever added to a `UACFGroupAIComponent` crashes in `SetGroupOwner`. The crash lands in a session
about performance, in a file nobody edited, caused by a change made months earlier.

(GS teams come per-character from `RaceTag` at `GSCharacterBase.cpp:269-272`, not from any group.)

---

## 4 — `UACFDamageHandlerComponent` is a CHARACTER component, not a damageable-thing component

"ACF already ships damage — stop hand-rolling `StructuralHealth`" is a correct-sounding instinct and it
is wrong for structures. Two hard dependencies on `UACFGASStatisticsComponent`, neither guarded:

- **Death.** BeginPlay binds the only thing that ever clears `bIsAlive` to the statistics component's
  `OnHealthReachesZero` (`ACFDamageHandlerComponent.cpp:45-49`). With no statistics component nothing is
  bound, `bIsAlive` stays at its constructor value **true** (`:34`), and `GetIsAlive()` answers "alive"
  forever. (Upstream the zero test is an exact float compare — `if (Data.NewValue == 0.f)`,
  `ACFGASStatisticsComponent.cpp:496`.)
- **Damage.** `TakeDamage` null-checks `StatisticsComp` on line **109** to decide whether to append the
  health stat, then calls `StatisticsComp->ConsumeStatistics(damages);` **outside** that check on line
  **113** — an unconditional dereference. The same function that carefully handles a missing component
  then dereferences it anyway.

So: add the damage handler to a barracks, a statue or a warren and the first sword swing **crashes**, not
"makes it invincible" (`vehicle-system:149` comes closest but names the wrong symptom). Survive that and
the building reports `IsAlive() == true` forever.

Characters are fine — every `AGSCharacterBase` is an `AACFCharacter`, which builds both components
(`ACFCharacter.cpp:72` StatisticsComp, `:76` DamageHandlerComp), and GS drives ACF's health attribute
clamped to exactly zero to satisfy the `== 0.f` test (`Attributes/GSAttributeSetBase.cpp:66-76`).
Structures are the exposure: `AGSSpawnerActor` keeps a bare float with its own entry point
(`AI/GSSpawnerActor.cpp:100`, `:109-121`, `:128-151`), and the shape repeats in
`Destruction/GSBreakableComponent` and `Destruction/GSDestructibleObjective`.

**Honest adoption for structures:** give them a `UACFGASStatisticsComponent` and an ASC too, or leave the
bespoke float alone.

---

## 5 — ACF never reads a team's attitude toward itself

```
// ACFTeamManagerSubsystem.cpp:175-177
if (TeamA == TeamB) { return GetDefaultAttitude(); }      // returns BEFORE the config is opened
```

`GetDefaultAttitude` reads `UPROPERTY(EditDefaultsOnly) TEnumAsByte<ETeamAttitude::Type> DefaultAttitude
= ETeamAttitude::Neutral;` (`AscentTeams/Public/Components/ACFTeamManagerComponent.h:69`) — **with no
setter anywhere in the class**. (Separately, cross-team rows are one-directional: the loop finds TeamA's
entry, does `Relationship.Find(TeamB)` and `break`s, `ACFTeamManagerSubsystem.cpp:186-194`. `teams`
already covers that half.)

`AGSGameState` creates the manager in C++ pointed at `DA_GSTeams` (`Core/GSGameState.cpp:12-32`),
described there as "Goblin and Human, hostile to each other and friendly to themselves, both
directions". `Combat/GSGameplayTags.h:143-147` gets the one-directional half right and then says
*"Same-team friendliness must also be authored"* — **authoring it achieves nothing**, the return happens
first. And because `GameStateClass` is bound to the C++ class `AGSGameState` (`Core/GSGameMode.cpp:36`)
rather than a Blueprint, the `EditDefaultsOnly` `DefaultAttitude` cannot be changed from Neutral by a
designer at all.

So every same-team query in Goblin Siege permanently answers **Neutral**, not Friendly:
`UACFTeamComponent::IsFriendlyWith` (`ACFTeamComponent.cpp:96-99`), `GetAttitudeTowardsActor`, and any
ACF or engine consumer of `ETeamAttitude` for a squadmate. Damage still behaves — `CanTeamDamageTeam`
handles the same-team case separately (`ACFTeamManagerSubsystem.cpp:212-214`) — which is exactly why
nobody notices, until someone builds "buff / heal / rescue my allies" or a perception ally-alert on
`GetAttitudeTowards`, finds zero allies, and spends a session editing `DA_GSTeams` rows the code never
reads.

The only real levers: change the manager component's `DefaultAttitude` default (needs a BP GameState or
a C++ default change), or do not ask ACF the question.

---

## 6 — A GameMode override on one map is a hard crash, not a warning

`UACFFunctionLibrary::GetBattleType` does
`return gameState->FindComponentByClass<UACFTeamManagerComponent>()->GetBattleType();` — GameState
null-checked, **component not** (`ACFFunctionLibrary.cpp:246-252`). Its one caller is
`AACFCharacter::IsMyEnemy`, on **every** hostility evaluation, before falling through to `AreEnemyTeams`
(`ACFCharacter.cpp:729-734`). ACF ships a safe sibling it does not use here:
`UACFTeamManagerSubsystem::GetBattleType` returns `ETeamBased` when the manager is missing
(`ACFTeamManagerSubsystem.cpp:148-154`).

The GS safety net is one line of C++ and one project setting: `AGSGameState` creates the component as a
default subobject (`Core/GSGameState.cpp:12`), `AGSGameMode` pins `GameStateClass = AGSGameState::StaticClass()`
(`Core/GSGameMode.cpp:36`), and `GlobalDefaultGameMode=/Game/Blueprints/BP_GSGameMode`
(`Config/DefaultEngine.ini:4`). `GSGameState.h:27-36` records why a component beats reparenting onto
`AACFGameState`: the subsystem only does `FindComponentByClass` on the GameState.

**A level designer can defeat all of that from World Settings without touching code.** Any map whose
GameMode Override is not `BP_GSGameMode` — a test map, a whitebox arena, an experimental Blueprint
GameMode — gets a GameState with no team manager, and the first hostility evaluation hard-crashes the
editor inside ACF. It presents as *"PIE crashes on this map only"* with an ACF call stack, which sends
the investigation into the plugin. **If a debug or test map is involved, check the GameMode override
first.**

---

## 7 — Death was migrated to ACF; revive was not

`UACFDamageHandlerComponent::Revive()` (a Server Reliable UFUNCTION) sets `bIsAlive = true` and calls
`StatisticsComp->OnRevive()` (`ACFDamageHandlerComponent.cpp:120-127`). It is the **only** writer of
`bIsAlive` back to true; `HandleStatReachedZero` (`:208-226`) is the only writer to false. `bIsAlive`
gates everything: `TakeDamage` returns 0 immediately while `!GetIsAlive()` (`:62-64`), and
`AACFCharacter::IsAlive()` is nothing but `GetDamageHandlerComponent()->GetIsAlive()`
(`ACFCharacter.cpp:482-484`) — which is what `UACFGroupAIComponent`'s roster, `HasAliveAgents` and
`GetAgentNearestTo` all read (`ACFGroupAIComponent.cpp:294`, `:539`).

Two sharp edges: `UACFGASStatisticsComponent::OnRevive_Implementation` has an **empty body**
(`ACFGASStatisticsComponent.cpp:506-508`), so `Revive()` restores no health; and `OnRep_IsAlive`
broadcasts `OnOwnerDeath` **unconditionally** on any replication of that bool
(`ACFDamageHandlerComponent.cpp:228-231`), i.e. clients run the death handler again on a revive.

GS's death half is fully adopted: `AGSCharacterBase` binds its consequences to
`DamageHandler->OnOwnerDeath` (`GSCharacterBase.cpp:279-285`) and drains ACF's health attribute clamped
to zero to fire it (`Attributes/GSAttributeSetBase.cpp:66-76`). The revive half is not:
`AGSCharacterBase::ApplyRespawnState` resets its own `bIsDead`, sets the ARS health attribute, clears
`State.Dead`, un-ragdolls, re-enables the capsule and movement (`GSCharacterBase.cpp:922-980`) and
**never touches the damage handler**. `grep -rn "Revive" Source/GoblinSiege` returns **zero hits**. This
is masked today only because `AGSGameMode::RespawnPlayer` leaves the corpse standing and `RestartPlayer`
produces a brand-new pawn with a fresh `bIsAlive` (`Core/GSGameMode.cpp:170-198`).

`ApplyRespawnState` is virtual and BlueprintCallable, so it reads like the project's general "bring this
character back" verb. Use it **in place** on an existing corpse — a downed ally picked up, a healer, a
debug un-kill, a boss phase reset — and you get a character alive to every GS check and dead to every ACF
one: full health, walking, `State.Dead` cleared, **permanently invulnerable** because `TakeDamage`
early-returns on `!bIsAlive` (and the ACF damage path is live behind `GSUseACFDamage`,
`GSGA_SwordLight.cpp:425-445`), and absent from any `UACFGroupAIComponent` roster.

**The fix is one line, not a recipe: `ApplyRespawnState` must call `ReviveCharacter`.**
`AACFCharacter::ReviveCharacter_Implementation` (`ACFCharacter.cpp:815-823`) already calls
`DamageHandler->Revive()`, writes health, unlocks actions and restores `MOVE_Walking` — so
`acf-core:90`'s `Character->ReviveCharacter(0.5f)` is the whole call. Do **not** hand-roll
`Revive()` + a health write; do note that `Revive()` *alone* leaves a living character at 0 HP that
nothing can ever kill again, because the zero test only fires on a **change** to 0.f.
