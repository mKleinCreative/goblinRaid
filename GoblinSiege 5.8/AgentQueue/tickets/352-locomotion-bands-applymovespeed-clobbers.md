---
id: 352
title: Locomotion bands: ApplyMoveSpeed clobbers the band with a stale BaseWalkSpeed snapshot, and Knight/Archer/Civilian still disarm their bands entirely
agent: claude-combat
status: done
claimed: 2026-08-29T05:00Z
build: none
waiting_on:
evaluated: 2026-08-29T05:17:58Z
observed: 2026-08-29T05:17:04Z | Measured every defender in PIE across an AI state change: patrol now reads MaxWalkSpeed 280.1 on the guards and 250.0 on Knight/Archer with TargetLocomotionState E_WALK, and driving one into combat via SetTarget moved it to AIState.Combat with MaxWalkSpeed 606.3 and band E_JOG. Before this, patrol and combat both read 606.3 and were indistinguishable. Nobody is frozen - no living non-Wait pawn reads 0. The 'Locomotion State inexistent' spam dropped from every service tick to 2 lines in 500, and the only pawn still disarmed is the player, which is #334's deliberate opt-out.
scenario: PIE on L_CombatArena with 7 placed defenders (2x CastleGuard01, 2x CastleGuard02, Knight, 2x Erika); sampled AIState, MaxWalkSpeed and GetTargetLocomotionState per pawn, then forced one controller into combat with SetTarget on the player.
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Content/Blueprints/Adversaries/BP_KnightDPelegrini.uasset
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
  - Content/Blueprints/Adversaries/BP_PeasantMan.uasset
---

## Goal

Locomotion bands: ApplyMoveSpeed clobbers the band with a stale BaseWalkSpeed snapshot, and Knight/Archer/Civilian still disarm their bands entirely

## Generate

Two defects behind one symptom - `Locomotion State inexistent` spamming every service tick, and
#334's leftover "patrol is indistinguishable from combat".

**1. `AGSCharacterBase::ApplyMoveSpeed` fought the bands.** `BaseWalkSpeed` is a one-time snapshot
taken in `BeginPlay`, *after* ACF applies its default state - so it captured the JOG speed (606.3 on
the guards), and every `ApplyMoveSpeed` then overwrote whatever band the AI state machine had just
selected. #334 proved the state machine works (`AIState.Wait` drove speed to 0, which only `EIdle`
can do) but measured 606.3 in BOTH patrol and combat; this is why.

Now, when bands are armed, the CURRENT BAND is the base and the multiplier modifies it: ACF owns
walk/jog/sprint, the attribute owns slows and buffs. Two authorities on one field with an explicit
order instead of a race. The player is untouched - `UGSCharacterMovementComponent` disarms its bands
by default, so the branch is skipped and the snapshot path (including the dodge speed lift) is
byte-identical.

**2. Knight, Archer and Civilian still emptied their bands.** #334 only ever cleared
`bDisarmLocomotionStates` on `BP_CastleGuard01/02`. The other three still ran
`LocomotionStates.Empty()` at `BeginPlay`, so every state request missed - and because
`UpdateMaxSpeed`'s miss branch never assigns `targetLocomotionState`, the request is never
remembered and `ACFUpdateCombatBTService` re-enters it every tick. Empty table + per-tick call =
constant log. Flag cleared on all three.

**3. `DefaultState` was EJog on every defender.** Found while verifying: after fixes 1 and 2 the
guards STILL patrolled at 606.3. `UACFCharacterMovementComponent::DefaultState` defaults to **EJog**
(`ACFCharacterMovementComponent.h:410`) and `BeginPlay` calls
`Internal_ApplyLocomotionState(DefaultState)` - which runs after the controller has already selected
EWalk for Patrol and overwrites it. Set to **EWalk** on all five adversary Blueprints.

Files: `Source/GoblinSiege/Characters/GSCharacterBase.cpp`;
`BP_KnightDPelegrini`, `BP_ErikaArcher`, `BP_PeasantMan` (disarm flag + DefaultState);
`BP_CastleGuard01`, `BP_CastleGuard02` (DefaultState only).

## Evaluate

**Measured before and after, on all seven placed defenders:**

| | Patrol | Combat |
|---|---|---|
| Before | 606.3 | 606.3 (indistinguishable) |
| After | **280.1** guards / **250.0** Knight+Archer, band `E_WALK` | **606.3**, band `E_JOG` |

Driving a controller into combat with `SetTarget` moved it `AIState.Patrol -> AIState.Combat` and
the speed followed. Nobody frozen: no living non-Wait pawn reads 0. Spam fell from every service
tick to **2 lines in 500**, and the only pawn still disarmed is the PLAYER - #334's deliberate
opt-out, because ACF's stock bands cap Jog at 500 while GS sprint needs >505.

**A NEAR MISS WORTH RECORDING, because it would have shipped silently.** My first version read
`GetCurrentLocomotionState()`. That is written only by `HandleStateChanged`, downstream of
`UpdateLocomotion`, which is gated on the mesh having a `UACFAnimInstance`
(`ACFCharacterMovementComponent.cpp:189`) - and **no Goblin Siege AnimBP is one** (`ABP_Human` is a
plain `Engine.AnimInstance`; `gs-locomotion-bands` §2). So it returns `EIdle` forever, band speed 0,
and `MaxWalkSpeed = 0 * multiplier = 0`: **every armed defender frozen solid.** It was caught by a
research agent reading the skill pack, not by me, and only after that did the compiler independently
reject the line for touching a protected member. The correct accessor is
`GetTargetLocomotionState()`, which `UpdateMaxSpeed` actually keeps current.

The armed-bands test also avoids the protected array: `GetCharacterMaxSpeedByState(EJog) > 0` is
public, and a disarmed table answers 0 for every state while a real jog band never does.

**AGENT_STATE.md owes:** DECISION - *ACF's locomotion bands are the authority on AI walk/jog speed;
ApplyMoveSpeed scales the current band rather than a BeginPlay snapshot, and every adversary
defaults to EWalk so patrol does not start at a jog.*

## Refine

**Changed in response to my own evaluation:** the accessor (`GetCurrent` -> `GetTarget`) and the
armed-bands test (protected array -> public speed probe). And fix 3 only exists because I verified
in PIE instead of trusting fixes 1 and 2 - both were correct and the guards still jogged on patrol.

**Deliberately left undone:**

1. **Knight and Erika carry ACF's STOCK band speeds** (250/500/650), not speeds measured from their
   own clips. The guards got 280.1/606.3 measured from the #333 retargets. So those two now move at
   state-driven speeds that do not match their animations - expect foot sliding. Measuring their
   clips is a small separate job.
2. **`Invalid Action Ability Tag` is NOT fixed, deliberately.** It is
   `UACFCombatBehaviourComponent::InitBehavior` calling `TriggerAction(EngagingAction, EHigh)` with
   an unset optional tag. ACF ships `EngagingAction` empty on **every one of its own controller
   Blueprints** and logs the same line in its own sample; `Internal_TriggerAction` bails on its first
   statement, so nothing is queued and no state is touched. Setting it to a tag the ability set does
   not grant would trade one log line for a silent no-op, which is worse. It only became audible
   because #343 made `InitBehavior` actually run.
3. **`UpdateLocomotion` remains dead** - no AnimBP is a `UACFAnimInstance`, so band `StateModifier`s
   never apply, `OnLocomotionStateChanged` never fires, and `GetCurrentLocomotionState()` still lies
   to anyone who reads it. Only the `SetLocomotionState -> UpdateMaxSpeed` path works. Anyone adding
   locomotion code here should read `gs-locomotion-bands` first.
4. **`SetLocomotionStateSpeed` is still a null-deref** on any disarmed character
   (`ACFCharacterMovementComponent.cpp:294`, unguarded `FindByKey(...)->StateModifier`). It is
   BlueprintCallable, so a designer can reach it. Only the player is disarmed now, which narrows the
   blast radius but does not remove it.
