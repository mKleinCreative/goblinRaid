---
name: gs-locomotion-bands
description: Goblin Siege — the disarmed ACF locomotion band table, the crash it guarantees, why the state machine never runs without a UACFAnimInstance, and the rules for migrating sprint.
globs: []
alwaysApply: false
---

# GS — ACF locomotion states, half-adopted

`UGSCharacterMovementComponent` derives from `UACFCharacterMovementComponent`
(`Source/GoblinSiege/Characters/GSCharacterMovementComponent.h:48`), so every GS character carries
ACF's locomotion band machinery — but two project decisions leave it in a state ACF never anticipated:
the band table is **emptied** on most characters, and **no GS Anim Blueprint derives from
`UACFAnimInstance`**. Read this before touching `MaxWalkSpeed`, before wiring a Blueprint speed node,
and before Ruling 27 / Phase 2b.

`character-controller` lists these APIs as plain runtime overrides. It does not say which of them
crash, which are inert, and which are the component's own output.

---

## 1 — `SetLocomotionStateSpeed` is a hard crash on our characters

```
// ACFCharacterMovementComponent.cpp:294
newState.StateModifier = LocomotionStates.FindByKey(State)->StateModifier;   // no null check
```

It is exposed to designers: `UFUNCTION(Server, Reliable, BlueprintCallable)` at
`ACFCharacterMovementComponent.h:331-332`. The sibling accessors `GetCharacterMaxSpeedByState`
(`cpp:307-314`) and `UpdateMaxSpeed` (`cpp:678-692`) **do** null-check the same `FindByKey`, so line
294 is an oversight, not a contract — do not "fix" ACF here, just don't call the node.

`UGSCharacterMovementComponent::BeginPlay` runs `LocomotionStates.Empty();`
(`GSCharacterMovementComponent.cpp:31`) whenever `bDisarmLocomotionStates` is set — default **true**
(`GSCharacterMovementComponent.h:64`). That is the player and every goblin. With an empty array
`FindByKey` returns nullptr for **every** state.

| Character | Band table | `Set Locomotion State Speed` |
|---|---|---|
| Player, all goblins | emptied at BeginPlay | null deref — instant crash |
| `BP_CastleGuard01` / `02` (author `LocomotionStates` + `bDisarmLocomotionStates`) | armed | crashes only for a state the Blueprint did not author |

Symptom: the first time a designer touches that Blueprint node the game dies with a call stack inside
ACF plugin code, which reads as "ACF is broken" rather than "our subclass emptied the table ACF
assumes is populated". The disarm comment block does not warn about it.

### The replication path zeroes MaxWalkSpeed (latent)

`UpdateMaxSpeed`'s miss branch is log-only ("Locomotion State inexistent",
`ACFCharacterMovementComponent.cpp:689-691`) and writes nothing — that is what
`GSCharacterMovementComponent.cpp:35-42` verified, and its comment calls the restore line "a no-op
today". Correct for the server, **wrong for a client**: `OnRep_LocomotionState` does not go through
`UpdateMaxSpeed`, it does

```
// ACFCharacterMovementComponent.cpp:352-357
MaxWalkSpeed = GetCharacterMaxSpeedByState(targetLocomotionState.State);   // returns literal 0.0f on a miss (cpp:307-314)
```

`targetLocomotionState` is `ReplicatedUsing = OnRep_LocomotionState` (header `:475-476`) and
`LocomotionStates` is itself replicated (`DOREPLIFETIME` at `cpp:57`). GS empties the table in
`BeginPlay`, which runs on clients too, and the "belt and braces" restore at
`GSCharacterMovementComponent.cpp:42` happens once and cannot help a later OnRep.

**NOT OBSERVED — the game is single-player today, so this is latent.** The day a second player exists:
remote pawns stop dead and slide, nothing is logged, and the retraction comment at
`GSCharacterMovementComponent.cpp:35-41` steers the investigation away from the real line.

---

## 2 — Without a `UACFAnimInstance` the classifier never runs

`UpdateLocomotion` — the function that classifies velocity into a band — is gated on the anim instance:

```
// ACFCharacterMovementComponent.cpp:189
if (GetOwner() && !IsFalling() && animInstance && !animInstance->IsAnyMontagePlaying())
```

`animInstance` is only ever set by `Cast<UACFAnimInstance>(Character->GetMesh()->GetAnimInstance())`
at `cpp:152-158` — the same block that calls `SetCurrentMoveset` / `SetCurrentOverlay` (`:155-156`).

Both GS Anim Blueprints have `NativeParentClass = /Script/Engine.AnimInstance`, read from their name
tables: `Content/Characters/Humans/ABP_Human.uasset` (used by `BP_CastleGuard01`) and
`Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset`. A recursive grep for
`ACFAnimInstance` across `Content/Characters`, `Content/Blueprints`, `Content/GoblinSiege` and
`Content/AI` returns nothing.

What is therefore dead on **every** GS character, guards included:

| Dead | Because |
|---|---|
| `currentLocomotionState` (frozen at `EIdle`) | `HandleStateChanged` (`cpp:390-415`) is its only writer and sits downstream of `UpdateLocomotion` |
| Band `StateModifier` — ACF's ARS hook, e.g. sprint stamina drain | applied only at `cpp:409` (`AddAttributeSetModifier`) |
| `OnLocomotionStateChanged` / `OnRep_CurrentLocomotionState` | never fire |
| `GetCurrentLocomotionState()` | lies |
| sprint-direction-cone auto-cancel (`cpp:207-213`) | lives inside `UpdateLocomotion` |

What still works, and is why the half-adoption looked fine: `SetLocomotionState` → `UpdateMaxSpeed`
(`cpp:274-288`, `678-692`) writes `MaxWalkSpeed` and `targetLocomotionState` only — which is why the
measurement "`SetLocomotionState(EWalk)` → 280.1, `(EJog)` → 606.3" reproduced.

The next person to author a band modifier, or bind `OnLocomotionStateChanged` to drive a bark or an
anim switch, gets nothing and has no error to chase — and will most likely fix it by writing more
custom code. ACF ships `ACF_SimpleTemplate_ABP` for exactly this (see `gs-anim-rig-compatibility`).

---

## 3 — ACF states are commanded, never earned

The constructor ships Idle 0 / Walk 250 / Jog 500 / Sprint 650
(`ACFCharacterMovementComponent.cpp:30-33`) with `DefaultState = EJog` (header `:409-410`). The
classifier promotes only inside a window —

```
// cpp:202
Speed > LowerStateSpeed + 5.f && Speed <= UpperStateSpeed + 5.f
```

— while `MaxWalkSpeed` is pinned to the **current** band's ceiling. So velocity can never reach the
next band on its own: the classifier can only *follow* a `SetLocomotionState` command, never lead it.
That is the mechanism behind "sprint is structurally impossible", and it applies to every band.

Sprint is a command: `SetLocomotionState(ESprint)` raises `MaxWalkSpeed` to 650, the classifier follows
the velocity up, and the server auto-cancels back to `EJog` when the character is not running
near-forward — `SprintDirectionCone` is **10 degrees** (header `:424-427`, check at `cpp:207-213`).

`CharacterMaxSpeed` (the max over the table) is recomputed only in `InitializeComponent` (`cpp:135`)
and `SetLocomotionStateSpeed` (`cpp:297`), and it is the divisor for the anim graph:
`NormalizedSpeed = SafeDivide(Speed, GetCharacterMaxSpeed())` (`ACFAnimInstance.cpp:281`).

GS does the opposite today: sprint is a speed write (`GSCharacterMovementComponent.h:19-21`,
`GSCharacterBase.cpp:30-40`) and the disarm exists to stop the bands overwriting it. Because
`targetLocomotionState` is never `ESprint`, `IsSprinting()` (header `:97-98`) is permanently false for
our sprinting player.

**Two rules for Phase 2b (Ruling 27, `GSCharacterMovementComponent.h:30-32`):**

1. You cannot promote a state by raising `MaxWalkSpeed`. Issue `SetLocomotionState`.
2. If bands are armed and something still writes `MaxWalkSpeed` **above the top band**, the window at
   `cpp:202` matches no band: `currentLocomotionState` sticks at its last value until a full stop
   (`cpp:198-200`), and `NormalizedSpeed` exceeds 1.0 so the locomotion blendspace runs off the end of
   its axis.
3. Expect the 10° cone to drop a migrated sprint the instant the player strafes — and note that on our
   characters that auto-cancel cannot even fire today, because it lives inside the animInstance-gated
   `UpdateLocomotion` (§2).

---

## 4 — `ApplyMoveSpeed` writes `MaxWalkSpeed` on characters whose bands are armed

With the machine armed, `MaxWalkSpeed` is ACF's **output**: BeginPlay applies `DefaultState`
(`Internal_ApplyLocomotionState`, `ACFCharacterMovementComponent.cpp:150`), and
`AACFAIController::UpdateLocomotionState` rewrites it on **every AI state transition**
(`ACFAIController.cpp:381-387`, called from `SetCurrentAIState` at `:363`).

GS writes it directly for every character:

```
// GSCharacterBase.cpp:305
MoveComp->MaxWalkSpeed = BaseWalkSpeed * ...GetMoveSpeedMultiplierAttribute()
```

bound to fire on every `MoveSpeedMultiplier` change (`:248-249`), on BeginPlay (`:286`) and from
`SetBaseWalkSpeed` (`:349-353`, called from the archetype path at `GSEnemyCharacter.cpp:110`).
`BaseWalkSpeed` is a one-shot snapshot at `GSCharacterBase.cpp:235-238`, taken **after** the movement
component's BeginPlay (`AActor::BeginPlay` dispatches component BeginPlay at
`Engine/Source/Runtime/Engine/Private/Actor.cpp:4819-4826`, before the derived actor's post-`Super`
code) — so on an armed guard it captures the **EJog** band value.

`GSCharacterMovementComponent.cpp:12` states the rule ("Nothing here may touch `MaxWalkSpeed`: owning
it is the whole point") but the base class violates it unconditionally, and the disarm flag cannot see
that.

Predicted sequence on `BP_CastleGuard01`: patrolling at `EWalk`/280.1 → guard blocks → `UGSGA_Block`
applies its `MoveSpeedMultiplier` GE (`GSGA_Block.cpp:84`) → the attribute delegate fires
`ApplyMoveSpeed` → both the apply and the expiry recompute from `BaseWalkSpeed` (~606, the jog
snapshot). The guard now patrols at jog speed while the component still believes it is `EWalk`, until
the next AI state transition happens to correct it. Symptom: a guard who walks his patrol correctly
until his first fight and jogs it forever after.

**This one is read from code, NOT OBSERVED.** `UGSGA_SwordLight` does the same class of thing with a
cache/restore pair (`GSGA_SwordLight.cpp:166-171`, `176-188`). Before adding another `MaxWalkSpeed`
writer, decide which system owns the value on that character.
