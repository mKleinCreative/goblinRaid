---
id: 334
title: Locomotion states: stop disarming ACF bands for AI so Patrol walks and Combat runs
agent: claude-acf
status: done
claimed: 2026-08-27T23:26Z
build: required
waiting_on: 
evaluated: 2026-08-28T23:25:58Z
observed: 2026-08-28T22:11:29Z | RUN HALF OBSERVED at last, in a real fight (GS.Horde.SpawnTest then GS.Raid.GotoActor to bring goblins to the guards). Three guards moved Patrol->Combat->Wait: A1 Combat@253s, A2 @259s, A3 @266s. The state machine works. BUT the band reads 606.3 in BOTH Patrol and Combat, so patrol is indistinguishable from combat. Wait->spd 0 DID apply, which proves LocomotionStateByAIState itself is working and pins the fault on the BaseWalkSpeed=606.3 jog snapshot: Patrol writes Walk 280.1 and ApplyMoveSpeed immediately overwrites it with BaseWalkSpeed x multiplier.
scenario: PIE on L_Tutorial_Island, 22:09, tick sampler recording every AI-state change with the movement components MaxWalkSpeed at that instant, across all 7 guards for ~70s of live combat
files: 
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.h
  - Source/GoblinSiege/Characters/GSCharacterMovementComponent.cpp
  - Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard02.uasset
  - Content/AI/BP_GSAIController_Militia.uasset
---

## Goal

Locomotion states: stop disarming ACF bands for AI so Patrol walks and Combat runs

## Generate

Michael: *"can't we just give them a walk and a run? A walk for when it's Quiet and a run for when the
battle begins?"* **ACF already ships exactly that** and we were switching it off.

`AACFAIController` carries `LocomotionStateByAIState` (`TMap<FGameplayTag, ELocomotionState>`), and
`SetCurrentAIState` calls `UpdateLocomotionState()` on every transition
(`ACFAIController.cpp:362-363`), which applies the mapped band. `ACFFollowSplinePathTask` even has a
per-path `LocomotionState`, so an individual patrol route can be authored walk or run.

It never fired because `UGSCharacterMovementComponent::BeginPlay` called `LocomotionStates.Empty()` on
EVERY character. With no bands the state lookup resolves to nothing, `MaxWalkSpeed` stays pinned at the
Blueprint's authored value, and every defender jogs everywhere at a flat 450uu/s. The disarm was written
for the PLAYER (ACF's stock bands cap Jog at 500 while sprint needs >505) but applied globally, including
to AI that never sprints. Same shape as the bugs in the 08-27 handoff: a workaround wrapped around ACF
that disables what ACF ships.

- `GSCharacterMovementComponent.h` - new `bDisarmLocomotionStates` (`EditDefaultsOnly`, default **true**,
  so the player's behaviour is byte-identical). Class comment extended to say why the disarm is
  player-only and that a Blueprint clearing the flag MUST author its own bands.
- `GSCharacterMovementComponent.cpp` - `BeginPlay` early-outs to `Super::BeginPlay()` when the flag is
  clear, leaving ACF's machine armed and never touching `MaxWalkSpeed`.
- **Deleted a comment that was actively lying.** It claimed that with the bands gone `Super::BeginPlay`
  "sets MaxWalkSpeed to ZERO" so the restore "is not optional". Handoff gotcha 3 flagged this; I
  re-verified it myself at `ACFCharacterMovementComponent.cpp:684-691` - the miss branch only logs
  `"Locomotion State inexistent"` and writes nothing. The restore is a harmless no-op and now says so.
- `BP_CastleGuard01` / `BP_CastleGuard02` - `LocomotionStates` authored: Idle 0 / **Walk 280.1** /
  **Jog 606.3** / Sprint 700. Walk and Jog are the measured ground speeds of the #333 retargeted clips,
  so each band plays its clip with no blend and no sliding.
- `BP_GSAIController_Militia` - `LocomotionStateByAIState`: `AIState.Patrol`->EWalk,
  `AIState.Combat`->EJog, `AIState.ReturnHome`->EJog, `AIState.Wait`->EIdle. Tag names come from ACF's
  `DefaultAIStateTags` - note EBattle's tag is **`AIState.Combat`**, not "Battle".

## Evaluate

**The mechanism is PROVEN LIVE, the wiring is not.** In PIE on `BP_CastleGuard01_C_1` I populated the
bands by hand on the running actor and measured `MaxWalkSpeed`:

    SetLocomotionState(EJog)  -> 606.2999877929688
    SetLocomotionState(EWalk) -> 280.1000061035156

Both land exactly on the clip speeds. That proves the bands drive `MaxWalkSpeed` once they exist.

**NOT observed, and the ticket must not close as if it were:** nothing has been compiled, so
`bDisarmLocomotionStates` does not exist on any asset yet and no guard has been seen walking on patrol
and running in combat. `UpdateLocomotionState` is not Blueprint-exposed, so I could not drive the AI
state from Python to test the map end-to-end - the 4 map keys are distinct (identical or empty tags
would collapse to 1), which shows the tags resolved, but that the map actually SELECTS the right band on
a state change is inferred from reading `ACFAIController.cpp`, not watched.

**Two gotchas hit, both already in the handoff:**
- `FACFLocomotionState.State` is `EditDefaultsOnly` and refuses `set_editor_property`; `import_text()`
  on the struct bypasses it (gotcha 6).
- `SetLocomotionState` early-returns when `targetLocomotionState` already equals the request, so my
  first test (EWalk, the default) looked like a no-op and nearly read as a failure.

**An ACF bug worth knowing.** `SetLocomotionStateSpeed_Implementation` dereferences
`LocomotionStates.FindByKey(State)->StateModifier` unguarded
(`ACFCharacterMovementComponent.cpp:294`). On a character whose bands were emptied that is a null deref -
a crash. Do not call it to repopulate bands; set the `LocomotionStates` array directly, which is
`BlueprintReadWrite`.


**UPDATE 2026-08-28 (claude-acf) - compiled, and the run half is now OBSERVED.** The refusal above
("nothing has been compiled... no guard has been seen walking on patrol and running in combat") is
superseded:

- Built into the 18:15 DLL. `bDisarmLocomotionStates` exists and is cleared on both guard Blueprints.
- **Real fight watched** (`GS.Horde.SpawnTest` + `GS.Raid.GotoActor` to bring goblins to the guards):
  three guards moved Patrol -> Combat -> Wait. A1 Combat@253s, A2 @259s, A3 @266s.
- **The map DOES select the band on a state change** - the thing this section said was inferred from
  reading `ACFAIController.cpp` rather than watched. `AIState.Wait` drove `MaxWalkSpeed` to **0**,
  which only the `EIdle` band can do.
- **But Patrol and Combat both read 606.3**, so patrol is indistinguishable from combat. Cause
  isolated: `GSCharacterBase::BeginPlay` (`:235-237`) snapshots `BaseWalkSpeed` AFTER ACF applies
  `DefaultState` EJog, capturing 606.3, and `ApplyMoveSpeed` (`:305`) then overwrites Patrol's Walk
  280.1 with `BaseWalkSpeed x multiplier`. Wait's 0 survives only because nothing re-triggers
  `ApplyMoveSpeed` after it.

**Closing as observed on Michael's call.** The band-selection mechanism this ticket set out to turn on
is working; the remaining `BaseWalkSpeed` ordering defect is a separate C++ fix and needs its own
ticket and a build.

## Refine

- Chose the opt-out flag over "skip the disarm when the owner is AI-controlled" because at `BeginPlay`
  possession is not guaranteed, so the controller test would be a race.
- Chose C++ over the Blueprint stopgap (repopulating bands in each guard's `BeginPlay`) on Michael's
  call. The stopgap would work today without a build but is a workaround stacked on a workaround, and
  ruling 27 already schedules deleting this class in Phase 2b.
- **Deliberately left undone:** the build, and therefore setting `bDisarmLocomotionStates=false` on the
  two guard BPs - that property does not exist until the DLL is rebuilt. The authored bands and state
  map are inert until then and harmless (they are emptied at BeginPlay exactly as before). This ticket
  cannot close until it is built AND a guard has been watched walking a patrol and breaking into a run.
- Untouched but noted: `GSAIControllerBase.h:13-16` still carries the same stale "our RunBehaviorTree
  calls remain load-bearing" claim, false since Phase 2a. Not mine to edit under this ticket.

> 2026-08-28T01:00Z Adopted by claude-acf (was claude-anims). Michael reassigned: this session has the 40 ACF skill packs listed, the previous one did not.
