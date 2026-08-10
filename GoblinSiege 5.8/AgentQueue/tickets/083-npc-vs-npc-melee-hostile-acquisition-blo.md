---
id: 083
title: NPC-vs-NPC melee: hostile acquisition, block AI, attack telegraph
agent: claude-npcfight
status: done
claimed: 2026-08-08T21:29Z
build: required
waiting_on:
evaluated: 2026-08-08T21:56:00Z
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_Block.h
  - Source/GoblinSiege/AI/Tasks/BTTask_Block.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.h
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - Source/GoblinSiege/Combat/GSGameplayTags.h
  - Source/GoblinSiege/Combat/GSGameplayTags.cpp
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
---

## Goal

NPC-vs-NPC melee: hostile acquisition, block AI, attack telegraph

## Generate

**The shape.** Everything that RESOLVES a hit was already faction-symmetric - the damage path, the
140 degree block arc in `GSDamageExecCalculation.cpp:139-165`, the guard break, hit-reacts, ragdoll,
and `IsHostileTo` on `RaceTag`. What was missing was three decisions no AI could make: *who is my
enemy* (hardcoded to the player), *is he winding up* (no signal existed at all), and *should I raise
my guard* (no caller). Each got the cheapest honest answer rather than a new system.

**The telegraph - `Combat/GSGameplayTags.{h,cpp}`, `Weapons/Abilities/GSGA_SwordLight.cpp`.** Two
new tags: `State.Attacking` (ActivationOwnedTags on the sword ability, spans the whole swing) and
`State.Attacking.Windup` (a LOOSE tag held for exactly `FGSSwingStage::WindupSeconds`). Set via a
file-static `GSSetWindupTelegraph` using `SetLooseGameplayTagCount(tag, 0/1)` rather than
Add/Remove - those are refcounted and this window has three exits (damage window opens, ability
ends, ability cancelled mid-windup by a death or dodge), so a double-remove would leave the count
negative and the NEXT swing's telegraph invisible. Raised in `RunStage`, cleared in
`OpenDamageWindow`, cleared again unconditionally in `EndAbility` so a cancelled swing cannot strand
it on a corpse. A zero-windup stage raises nothing at all - unreactable by design, which is what the
guard break wants.

Deliberately NOT an anim notify: `GSGA_SwordLight.h` records that these timings are tuned constantly
and that structural montage edits crash the editor with its window open, and a notify pass would
cost a montage edit per weapon per skeleton. The per-stage `WindupSeconds` already *is* this window.

**Acquisition - `AI/Tasks/BTService_AcquireTarget.{h,cpp}`.** The player-specific part was one line
(`UGameplayStatics::GetPlayerPawn(this, 0)`); the dead-target rejection, the clear-on-out-of-range
and the stand-off ring maths were already target-agnostic and took #006/#008 to get right. So the
SELECTION RULE was replaced in place rather than the node. Now: nearest live hostile by
`IsHostileTo` - the same predicate the melee sweep uses, so an AI cannot chase something it cannot
damage. Added node memory (`FGSAcquireTargetMemory`), hysteresis (0.75x discount on the incumbent, or
two defenders between two goblins oscillate forever), `LoseRadius` 2000 separate from `AcquireRadius`
1500 (was 3000 - at that range every defender in the hamlet acquired the first goblin to arrive), an
immediate rescan when a target is dropped, and `bSelectTarget` so `BT_HordeGoblin` can reuse the node
for slot+telegraph only while `AGSHordeAIController` keeps owning `TargetActor`.

A candidate must carry a VALID `RaceTag`. `IsHostileTo` treats "no opinion" as hostile, which is
right for damage (nothing becomes accidentally immune) and wrong for acquisition -
`BP_GS_TargetDummy` has no `RaceData` and every defender in the level would have jogged to it. Damage
semantics untouched.

`Interval` 0.5 -> 0.15s so a 0.22s windup cannot fall between samples, with the O(actors) scan
throttled separately by `ReacquireIntervalSeconds` 0.5. The telegraph is LATCHED (0.45s) rather than
sampled, keyed to the target's `GetUniqueID` so a latch cannot survive a target switch.

Replaced #008's FName-hash jitter with an 8-slot snapped lattice at `StandoffRadius` 180: adjacent
slots are now 2*180*sin(22.5) = 138uu apart, which is the ~137uu capsule-touch #008 measured and
recorded its own 89uu as too small. Snapping keeps the property that made the hash version work -
you approach from the side you are already on, so nobody crosses the pack.

**Blocking - `AI/Tasks/BTTask_Block.{h,cpp}` (the ONE new UCLASS).** Latent, so `OnTaskFinished`
drops the guard on abort as well as completion - a fire-and-forget call plus a timer leaks a
permanent guard the first time a branch is aborted. Rolls `TelegraphBlockChance` 0.55 when
`TargetIsAttacking` is set, else `IdleBlockChance` 0.10; holds 0.9s +0-0.3; 1.0s +0-0.4 cooldown
after. Faces the target at the character's own `GetTurnRateRadPerSec` - its first AI-side consumer -
because the block arc is frontal and these pawns run `bOrientRotationToMovement` with
`bUseControllerRotationYaw` false, so a stationary blocker never turns and an unfaced guard mitigates
nothing. Turning at a rate rather than snapping is what keeps flanking meaningful.

`DeclineCooldownSeconds` 0.35 is the least obvious and most load-bearing number here: without it the
tree re-rolls several times inside one windup and n rolls at p compound to 1-(1-p)^n, so three ticks
turn 0.55 into 0.91 and the defender blocks essentially everything.

**Guard break - `AI/Tasks/BTTask_MeleeAttack.{h,cpp}`.** Folded into the melee node rather than given
its own: same question ("what do I do at melee range"), and a separate node would duplicate the
range, facing and cooldown bookkeeping. Fires before the light attack on `IsBlocking()` at 0.7 within
200uu, 4s cooldown, and stamps BOTH clocks so a defender cannot kick and immediately swing. The
asymmetry is the design (`GSCharacterBase.h:228-231`): `CanGuardBreak()` is a null check on the
ability class, so allied goblins skip the branch for free and must flank or out-wait a guard.

Also capped the facing snap at `MaxFacingSnapDegrees` 120. It was an unconditional
`SetActorRotation`, which made every defender a turret that could snap 180 degrees and swing in the
same frame - flanking did nothing and no attack could be made to whiff.

**Goblins can open a fight - `Horde/GSHordeSubsystem.{h,cpp}`.** `RegisterThreat` was fed only by the
three Frenzy damage hooks, both of which are retaliation, so a militiaman standing three metres away
who had not yet hit anybody was never a threat and a warband would walk past the garrison. Added
`ScanForThreats` on a 0.5s timer: ONE sweep for the whole crowd (per-pawn checks would be per-agent
sensing wearing a different hat, which is exactly what GDD 3.4 and this class exist to avoid),
anchored on every live goblin AND its summoner, registering non-goblin valid-race hostiles within
`AutoThreatRadius` 1200 - equal to `SightRadius`, so neither faction gets a structural first strike.

**Tooling - `AI/GSAIDebug.h` (new), `Combat/GSDebugCommands.cpp`.** `GS.Combat.LogAI` and a
`GSAIDebug::Log` shared by the three deciding TUs, added to the `GS.PlayerView` toggle table.
`GS.Combat.LogDamage` answers "what happened to that hit"; it cannot answer "why did he not block",
which has six causes that are indistinguishable from outside.

`GS.Combat.Duel [n] [classA] [classB] [raceBAsset] [raceBRow]` - there was NO way to put the game
into a two-AI-fighting state at all (`GS.Raid.SpawnAt` only redirects the player spawn, and every
map's placed defenders are one faction). It exploits the fact that
`AGSEnemyCharacter::InitializeFromArchetype` is public and sets `RaceTag`: side B is an ordinary
human defender Blueprint re-badged onto `DA_Race_Goblin` AFTER it spawns, keeping its mesh, its
abilities and the `BT_Militia` it was already possessed with. **No goblin pawn, animation or
behaviour tree has to exist for the melee to be exercised end to end.**

**Editor-side (done, saved, verified by read-back):**
- `/Game/AI/DA_Race_Goblin` - `RaceTag = Race.Goblin`; row `HordeGoblin` (40 HP, the number
  `GSHordeGoblin.h` documents) and row `Brawler` (75/6 mirroring Knight, `BT_Militia`), the latter
  existing purely so `GS.Combat.Duel` needs no new pawn. Authored via `import_text` on the struct -
  every `FGSArchetypeDefinition` field is `EditDefaultsOnly` and Python's property setter refuses
  those on a struct instance.
- `/Game/AI/BB_Human` += `TargetIsAttacking` (Bool). The concrete key-type classes are not exposed to
  Python; `unreal.load_class(None, '/Script/AIModule.BlackboardKeyType_Bool')` is the way in.

**Cleared #069's owed check** (its own step 2, never done): all seven adversary Blueprints kept all
four ability slots across the hoist of the ability UPROPERTYs from `AGSEnemyCharacter` down to
`AGSCharacterBase`. Read off each CDO - `GA_GS_SwordLight_C / SwordHeavy_C / GuardBreak_C / Block_C`
present on every one, `DA_Race_Human` + correct row on the six defenders. #048's precedent was not
needed. Also confirmed `BP_GS_TargetDummy` has no `race_data`, which is what motivated the
valid-RaceTag rule above.

## Evaluate

**NOTHING IN THIS TICKET HAS COMPILED OR RUN.** The gate was closed by this ticket itself; Michael's
ruling was to write G/E/R, close it, and build under a fresh ticket. Every C++ claim below is static
reasoning. The editor-side claims are the exception - `DA_Race_Goblin` and the `BB_Human` key were
written and read back in the running editor, so those two are evidence.

**What I expect to be wrong at first compile**, in likelihood order. Recorded before the build so the
list cannot be quietly rewritten to match the errors:
1. `GS.Combat.Duel` uses `GSHordeGameWorld`, `UGameplayStatics` and `GEngine` from earlier in
   `GSDebugCommands.cpp`. The include block I appended may duplicate or miss one - `SCENE_QUERY_STAT`
   and `ECC_WorldStatic` in particular are pulled in transitively today and I did not verify it.
2. `FBlackboardKeySelector::AddBoolFilter` signature, and whether `IsSet()` is the right emptiness
   test on an unconfigured selector.
3. `AGSHordeGoblin` is forward-declared in `GSHordeSubsystem.h`; `ScanForThreats` calls `IsAlive()`
   and `GetActorLocation()` on one, which needs the full type. The .cpp does include it.
4. `TActorIterator` over `AGSCharacterBase` - `EngineUtils.h` added to both TUs, but this is the kind
   of include that is already transitively present until it is not, in exactly one of them.

**Unverifiable without PIE, and the honest risk in each.** The block probabilities are arithmetic
against numbers nobody has watched: 0.55 telegraph / 0.10 idle / 0.9s hold / 1.0s cooldown / 0.35s
decline. The failure mode I most expect is **mutual turtling** - two reaction-blockers holding guards
at each other until the raid ends - and the counters (guard break at 0.7, the ~50% guard-uptime cap
from hold-vs-cooldown, and goblins having no guard break at all) are all untested. The research bands
to judge against: 3-6 blocked exchanges per kill, TTK 10-18s, no fight over 45s.

Second most likely: `BTTask_Block` faces its target every tick while `bOrientRotationToMovement` is
also live. I argued velocity is ~0 during the block branch so movement rotation does not fight it,
but that is reasoning, not observation.

**Not done, and needed before any of this is visible:** `BT_Militia` has no Block node yet, so the
new task is unreachable. That is post-build editor work and must go through Python - the asset has no
EdGraph and opening it in the BT editor risks wiping the tree.

**Touched outside the goal:** the `MaxFacingSnapDegrees` cap changes how defenders fight the PLAYER,
not only each other - previously they always turned to face before swinging and now they can be
flanked. That is the intended direction but it is a live change to existing player-facing behaviour
and should be judged in PIE, not assumed.

**Owed to `AGENT_STATE.md`:** a DECISION line that `State.Attacking.Windup` is the ONLY sanctioned
channel for an AI to learn a hit is coming - no BT node may read an opponent's ability internals,
montage position or blackboard - because that is the line between a fair fight and a psychic one, and
it is exactly the shortcut that gets added at 1am. Also a BUILT line for the two editor assets, and a
correction that #069's step-2 ability-slot check is now done and clean.

## Refine

**Three things changed in response to writing the above.**

1. `FMath::ClampAngle` -> `FMath::Clamp` in `BTTask_Block::TickTask`. `FindDeltaAngleDegrees` already
   returns a shortest signed turn in [-180,180]; `ClampAngle` re-normalises and was the wrong tool.
   Would have produced a subtly wrong turn rate rather than an error, which is the worst kind.
2. Added the immediate rescan when a target is dropped. Without it a defender that had just killed
   someone stood over the body for up to `ReacquireIntervalSeconds` before noticing the next goblin -
   reading as losing interest at the exact moment it should not.
3. `SetLooseGameplayTagCount` instead of the Add/Remove pair for the telegraph, once I counted the
   exit paths and found three. Idempotent-by-construction beats symmetry here.

**Deliberately left undone.** Attack tokens: a real per-victim permission pool needs a shared arbiter
and another new UCLASS, i.e. another six-minute build, for a problem that only appears at 3v3+. The
8-slot ring plus the existing 1.2 + 0-0.6s per-agent cooldown should hold concurrent attackers near 2
at 3v3; if it does not, the follow-up is a cap of 2 and it is its own ticket. Also left: the
`OnAttackBlocked` delegate from the original plan - the guard break reads `IsBlocking()` directly,
which is simpler and more immediate than counting consecutive blocked hits, so the delegate earned
nothing. Stamina on AI actions stays out (`UGSStaminaComponent` is player-only with zero C++
callers); `BlockCooldownSeconds` is the stand-in that bounds guard uptime. And the
class-identity-as-faction cleanup (`GSDamageExecCalculation.cpp:120-126`, `GSFireVolume.cpp:394`,
`GSBuffAuraComponent.cpp:48`) stays out because fixing it changes fire, aura and soft-lock behaviour
for the player all at once - its own ticket. The accepted consequence is that goblin-vs-human damage
is flat, with no race multiplier applied. Fine for a first fight.
