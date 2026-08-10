---
id: 069
title: Allied goblin horde: pool subsystem, stimulus bus, horn on MMB, BT runner, combat verbs hoisted to GSCharacterBase
agent: claude-horde
status: done
claimed: 2026-08-07T16:03Z
build: required
waiting_on:
evaluated: 2026-08-07T18:44:51Z
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeGoblin.h
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
  - Source/GoblinSiege/Horde/GSHordeAIController.h
  - Source/GoblinSiege/Horde/GSHordeAIController.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.h
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.cpp
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Characters/GSEnemyCharacter.h
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Alarm/GSAlarmTypes.h
  - Source/GoblinSiege/Combat/GSDebugCommands.h
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_TorchToss.cpp
---

## Goal

Allied goblin horde: pool subsystem, stimulus bus, horn on MMB, BT runner, combat verbs hoisted to GSCharacterBase

## Generate

**The dependency root.** `Horde/GSHordeSubsystem.{h,cpp}` (new) — `UWorldSubsystem` copying
`UGSRaidDirector`, the only one of the three existing subsystems that keeps BOTH the
`ShouldCreateSubsystem` game-world gate and a static `Get()`. Pool is `ActiveCap=10`,
`ReserveMultiplier=2`, `GetRaidPoolSize()` derived — the 20 is never typed as a literal. Three pool
exits kept distinct, which is the part most likely to be "simplified" later into a bug:
`SummonWave` is the ONLY debit (decision 40); `NotifyGoblinDied` removes from active and credits
nothing back (the goblin was already paid for at spawn — debiting again double-charges);
`NotifyCourierDelivered` credits exactly one per GOBLIN, not per cargo; `NotifyGoblinSpentOnWarren`
is a fourth state that is neither a death nor a return. `ActiveGoblins` is keyed by summoning
`AController` from day one. A threat registry (`RegisterThreat` / `GetAssignedTargetFor`) replaces
per-goblin perception. Arrival reuses `AGSRaidMarker::GatherByType(Marker.HordeArrival)` — no new
marker UCLASS, see Evaluate.

**Combat verbs hoisted.** `Characters/GSCharacterBase.{h,cpp}` gained `TryLightAttack`,
`TryHeavyAttack`, `TryGuardBreak`, `StartBlocking`, `StopBlocking`, `CanGuardBreak`, the four
ability-class UPROPERTYs, and `GrantIfSet` / `TryActivate` / `GrantCombatAbilities()`.
`Characters/GSEnemyCharacter.{h,cpp}` lost all of it and now calls `GrantCombatAbilities()` in
`BeginPlay`. `AI/Tasks/BTTask_MeleeAttack.cpp` widened its cast from `AGSEnemyCharacter` to
`AGSCharacterBase`; the archetype cooldown lookup became a soft cast so a goblin falls back to
`DefaultAttackCooldownSeconds`.

**Frenzy triggers.** `FGSOnDamaged(Attacker, Damage)` and `FGSOnDealtDamage(Victim)` on
`AGSCharacterBase`, plus `NotifyDealtDamage()`. In `HandleHealthChanged` the
`PendingDamageInstigator` read moved ABOVE the hit-react branch — it used to sit inside the
`HitReactMinDamageFraction` gate, so any hit too small to stagger never named its attacker.
`OnDamaged` fires before `HandleDeath`, so a killing blow still identifies the killer.
`GSGA_SwordLight.cpp` calls `NotifyDealtDamage` after the effect is applied.

**Perception stripped from the horde.** `AI/GSAIControllerBase.{h,cpp}` now takes an
`FObjectInitializer` and guards every perception line on the component surviving;
`AGSHordeAIController` passes `DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent"))`.

**The pawn and its brain.** `Horde/GSHordeGoblin.{h,cpp}` — RVO (all three lines;
`bUseRVOAvoidance` alone is a no-op because `AvoidanceWeight` defaults to 0), `RaceData` /
`ArchetypeRowName` so 40 HP is data, a `HandleDeath` override into the pool, three Frenzy bindings.
`Horde/GSHordeAIController.{h,cpp}` — runs `CompanionBehaviorTree`, feeds the blackboard from the
subsystem on a 0.2s timer, and the tick-follow is DELETED. New `UENUM EGSHordeState`.

**The horn.** `Weapons/Abilities/GSGA_Horn.{h,cpp}` (new) — `ServerOnly`, `InstancedPerActor`,
blocks on its own `State.Horn` and on `State.Carrying`; raises the alarm to `Raid` on the first
frame of the blast and summons at the end. `GSGameplayTags` gained `State.Horn`; `GSAlarmTypes.h`
gained `EGSAlarmSource::HornBlast`, **appended** not inserted. `GSPlayerCharacter` gained
`HornAction`, `HornAbilityClass` (C++-defaulted), a guarded `BindAction`, and `Input_Horn`.

**Debug.** `GS.Horde.SpawnTest [n]` and `GS.Horde.Status` appended to `Combat/GSDebugCommands.cpp`,
routed through `SummonWave` so the test exercises the pool rather than bypassing it.

## Evaluate

**NOTHING HERE HAS COMPILED OR RUN.** The build gate was closed for this ticket's entire life
(#062 and #067 live per Michael; #070 closed at his instruction near the end). Every claim below is
static reasoning, not evidence. That is the single most important fact about this ticket.

**Three defects found in my own work and fixed before hand-back.** All three would have been build
failures, and catching them by reading rather than by compiling was luck as much as method:

1. `AGSPlayerCharacter` already declared `BlockAbilityClass` and `GuardBreakAbilityClass`. Hoisting
   those exact names into `AGSCharacterBase` puts two UPROPERTYs of one name in one hierarchy,
   which UnrealHeaderTool rejects. Removed the player's declarations; it inherits them now.
2. `UFUNCTION`s cannot take `const T*` parameters. `GetAssignedTargetFor`, `GetFollowTargetFor` and
   `GetFollowSlotFor` all did. Widened to non-const, with a comment saying why.
3. `UGSGA_Horn` originally cached a raw `const FGameplayAbilityActorInfo*` across an
   `AbilityTask_WaitDelay`. For an ability whose whole purpose is being blown mid-fight, the avatar
   dying during the blast is not a rare case. Replaced with `GetCurrentActorInfo()` at point of use.

**Two claims written into code comments that I could NOT verify, and that must be checked in-editor
before any PIE result is trusted:**

- That the six adversary Blueprints keep their four ability assignments across the hoist. UE
  resolves serialised properties by name within a hierarchy and only the declaring class moved, so
  this *should* hold — but #048 is precedent in this repo for exactly that assumption failing.
  Open `BP_CastleGuard01` after the build.
- That `BP_GSPlayerCharacter` keeps its block / guard-break values, for the same reason. That asset
  is held by #062, so I could not even look at it.

**Scope touched outside the goal, deliberately:** the `bDetectFriendlies = false` line in
`GSAIControllerBase` got a comment recording that it has never worked — affiliation resolves
through `IGenericTeamAgentInterface`, which nothing in this project implements, so every actor
reads as neutral. Left functionally as-is; adding a team interface would create a second source of
truth competing with `RaceTag` / `IsHostileTo`.

**AGENT_STATE.md owes four DECISION lines.** Not written here — that file is not in this claim:

- The war-horn is on MIDDLE MOUSE, not G. G is `IA_Block` from #058, Michael's own ask. Both GDDs
  saying "G" is an erratum, as is §2.3's "torch toss (Q)" (`IA_ThrowTorch` has had no IMC row since
  the torch became a held weapon).
- The horde arrives from a portal / Warren mouth, NOT the treeline (Michael, 2026-08-07). GDD §2.5's
  treeline sprint is superseded — a later agent must not "restore" it. This also sidesteps
  `GEN_NavBounds_Village` being 4000x4000 uu.
- Combat verbs live on `AGSCharacterBase`. Reparenting `AGSHordeGoblin` to `AGSEnemyCharacter` is
  the tempting one-liner and silently flips five class-identity checks: `GSFireVolume.cpp:394` would
  stop burning the horde, `GSTargetingComponent.cpp:50` would soft-lock the player onto his own
  goblins, `GSBuffAuraComponent.cpp:48` would let defender auras buff them.
- `AGSHordeSpawnMarker` will NOT be built. `AGENT_STATE.md:160` lists it as missing work; the tag
  and `GatherByType` already exist, and `GSRaidMarker.h:20-26` argues against new marker UCLASSes.
  That line is stale architecture, not a work item.

**Also owed, and not mine to write:** `HANDOFF.md` Part 1's nine "nobody has fixed these" findings
are all already fixed (#036 / #037 / #038), and `AGENT_STATE.md:8-17` is a banner every agent reads
at run start still asserting they are open. Verified one directly this session:
`GSArrowProjectile.cpp:150` already calls `IsHostileTo`, so the "arrows ignore RaceTag" entry at
`AGENT_STATE.md:174` is false.

## Refine

**Deliberately left undone: AI vault / mantle (decision 41-a).** Michael's call, 2026-08-07. It
needs `BP_GSPlayerCharacter` (held by #062), the traversal system is being actively rebuilt by #067
and #070, and vault / mantle / climb exist only as Blueprint graphs entered through Enhanced Input
that an AI cannot press — there are zero `NavLink` / `NavArea` hits repo-wide. Building AI traversal
against a system two other tickets are mid-rewrite on would have been wasted work. `Stranded`
therefore degrades to "unreachable → idle → re-horn free → trudge home", which repo GDD §5 already
describes. Raise as its own ticket once the climb rebuild settles.

**Also not done, and not horde work:** couriers, livestock, prisoners, and the Warren's
banking / respawn half. Each needs a feature that does not exist as an actor at all. The subsystem
carries `NotifyCourierDelivered` and `NotifyGoblinSpentOnWarren` so the pool accounting is correct
the day those land, but nothing calls them yet.

**Changed in response to my own evaluation:** the three defects above, plus routing
`GS.Horde.SpawnTest` through `SummonWave` rather than spawning directly — a test that bypassed the
pool would have proved the pawn renders and proved nothing about the system under test, and would
happily have produced eleven goblins against a cap of ten.

**The first three things to do when the gate opens**, in order: build editor-closed; open
`BP_CastleGuard01` and confirm its four ability slots survived; then `GS.Horde.Status` in PIE to
confirm the subsystem exists at all before blaming anything else. Note that until `BP_HordeGoblin`
and `DA_Race_Goblin` exist (Tier 2, editor work) `SummonWave` will correctly refuse and log that
`HordeGoblinClassPath` is unset — that is the designed message, not a failure.
