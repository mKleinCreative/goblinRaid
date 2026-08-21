---
id: 223
title: ACF Phase 2a: AGSCharacterBase reparents to AACFCharacter, one ASC
agent: claude-acf
status: done
claimed: 2026-08-21T00:53Z
build: none
waiting_on:
evaluated: 2026-08-21T01:04:16Z
observed: 2026-08-21T01:47:31Z | Phase 2a complete and watched. Reparent onto AACFCharacter with a single ASC: 3 goblin deaths with the pool logging Reserve N unchanged, 63 telegraphs, 11 block/PUNISH events, and after the tick fix sprint and stamina both work again. The cause of the sprint/stamina outage was ACFCharacter.cpp:83 setting PrimaryActorTick.bStartWithTickEnabled=false, which killed every tick-driven Blueprint system while leaving all event-driven ones working.
scenario: L_CombatArena, 2026-08-21: horde fight with defenders for combat, then Michael driving the player pawn for sprint and stamina after the tick build.
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/GoblinSiege.Build.cs
---

## Goal

ACF Phase 2a: AGSCharacterBase reparents to AACFCharacter, one ASC

## Generate

`AGSCharacterBase : ACharacter, IAbilitySystemInterface` -> **`AACFCharacter`**. Build succeeded.

Six things the reparent forced, each found by the compiler rather than predicted:

1. **`IAbilitySystemInterface` dropped from our declaration.** `AACFCharacter` already declares it
   (with `IGenericTeamAgentInterface`, `IACFEntityInterface`, `IALSSavableInterface`); UHT rejects a
   re-declaration in a derived class.
2. **We no longer create an ASC.** `AACFCharacter`'s constructor builds **fourteen** components
   including `ActionsComp` (a `UACFAbilitySystemComponent`) and `StatisticsComp`, and its
   `GetAbilitySystemComponent()` returns `ActionsComp`. Our member survives **as a cached pointer**
   assigned in `PostInitializeComponents`, so ~20 direct call sites compile unchanged and there is
   exactly ONE ASC. `UGSAttributeSetBase` stays a default subobject of the actor - that is how it
   reaches the ASC at all, since `UAbilitySystemComponent::InitializeComponent` walks the owner's
   default subobjects and registers every `UAttributeSet` it finds.
3. **`GetAbilitySystemComponent` re-exposed as public.** ACF declares it **protected**, which broke
   four external callers (`GSBuffAuraComponent`, `GSWeaponComponent`, `GSObjective_KillLandlord` x2).
   Ours forwards to `Super`, so it changes access and nothing else.
4. **`bAutoInit = false`** via `SetAutoInit(false)`. It defaults true, which would make
   `UACFCharacterInitializerComponent` apply a `UACFCharacterDataAsset` we have not authored for a
   single character. The acf-core skill's failure table names the outcome exactly: *"Characters have
   no stats / zero health."*
5. **The whole constructor chain now takes an `FObjectInitializer`** - `AACFCharacter` has no default
   constructor (`ACFCharacter.h:54`). Four classes: base, enemy, player, horde goblin.
6. **`AscentSaveSystem` added to `Build.cs`.** Deriving from `AACFCharacter` made UHT emit our
   class's `IALSSavableInterface` thunks into our module, and the link failed on
   `OnSaved_Implementation` / `ShouldBeIgnored_Implementation`. **This is the trap `Build.cs`'s own
   comment already describes** - the include resolved transitively, everything compiled, and only the
   linker knew. Same family as #215.

Two name collisions resolved, both checked against Content before deciding:

- **`IsAlive`** - both classes declared `UFUNCTION(BlueprintPure) bool IsAlive() const`. Ours keeps
  the body and loses the `UFUNCTION`. Zero `.uasset` files reference `IsAlive`; 21 C++ call sites do.
- **`GetTargetingComponent`** - same name, different return type (`UGSTargetingComponent*` vs
  `UATSBaseTargetComponent*`), so it could not be an override. Renamed to `GetGSTargeting`. Zero
  callers in Source, zero in Content.

## Evaluate

**BUILT, NOT RUN. Nothing about behaviour is claimed.** The binary is 2026-08-20 18:03.

**THIS IS A WAY-STATION, NOT THE MIGRATION.** Phase 2a deliberately leaves ACF's own systems dormant
so there is something watchable tonight. What is now true and must not be mistaken for finished:

- **ACF's `IsAlive()` answers "alive" for a corpse.** It reads
  `GetDamageHandlerComponent()->GetIsAlive()`, whose `bIsAlive` defaults to true and is only cleared
  by ACF's own damage path, which nothing drives. Ours is the truth. This matters the moment ACF's
  targeting or `IACFEntityInterface::IsEntityAlive` is consulted - **Phase 2b must route death
  through `UACFDamageHandlerComponent` and then delete ours**, not keep two.
- **`StatisticsComp` exists on every character and is uninitialised.** Rulings 25 and 35 are NOT
  satisfied by this ticket.
- **Fourteen new components now sit on every character**, including `UACFRagdollComponent`,
  `UACFEffectsManagerComponent`, `UACFEquipmentComponent`, `UMotionWarpingComponent` and a
  `UAudioComponent`. None are configured. Whether any of them fights our own ragdoll, death or
  effects handling is **unknown and unwatched** - it is the first thing a PIE session should look for.

**What was verified before deciding, rather than assumed:** every `.uasset` in Content was searched
for `IsAlive` and `GetTargetingComponent` before either name was changed. Both returned zero.

## Refine

Kept the `AbilitySystemComponent` member name rather than renaming ~20 call sites to
`GetAbilitySystemComponent()`. A reparent that also touches twenty unrelated lines is a reparent
nobody can review, and the member is now `UPROPERTY(Transient)` with a comment saying it is cached
rather than owned.

Deliberately NOT done: the attribute migration (2b) and the class-identity audit (2c, 8 casts).

---

## Addendum - the movement component (Michael's call, 2026-08-21)

**Sprint broke the moment 2a shipped, and it was not the only thing.**

`AACFCharacter`'s constructor swaps the movement component for `UACFCharacterMovementComponent`
(`ACFCharacter.cpp:64`). That component's `TickComponent -> UpdateLocomotion` classifies the pawn's
velocity into a locomotion band **every frame** and rewrites `MaxWalkSpeed` from its
`LocomotionStates` table - which ships POPULATED, verified by reading the live CDO rather than
assumed:

| state | max speed |
|---|---|
| Idle | 0 |
| Walk | 250 |
| Jog | **500** |
| Sprint | 650 |

`DefaultState = EJog`, so `BeginPlay`'s `Internal_ApplyLocomotionState` pins `MaxWalkSpeed` at 500.
**Sprint is then structurally unreachable**: entering the Sprint band needs velocity above 505 and
the cap is 500. ACF expects sprint to be `SetLocomotionState(ESprint)` - a STATE change - not a speed
write. Every other move-speed effect we own (block slow, carry slow, per-swing `MoveSpeedScale`,
`ApplyMoveSpeed`) was being overwritten every frame too; sprint was simply the one that got noticed.

**Fix taken: undo the swap.** `AGSCharacterBase`'s constructor passes
`SetDefaultSubobjectClass<UCharacterMovementComponent>`, so Phase 2a is movement-neutral.

**A DEFERRAL, NOT A REJECTION.** Ruling 27 already says move speed becomes ACF locomotion states.
Phase 2b does it alongside the ARS attribute migration, so the question "how do carry + block + swing
compose when the target is discrete states" gets answered ONCE, with the attribute work, rather than
twice.

Safe because `AACFCharacter::PostInitProperties` only logs a warning when the cast fails
(`ACFCharacter.cpp:107`) and every other `LocomotionComp` use in that class is null-guarded.
**Expect one "Your Character Movement component MUST BE an ACFCharacterMovementComponent" warning per
character until 2b - noise, not failure.**

Build succeeded. Still NOT watched.

## Addendum 2 - the first fix was silently refused

`SetDefaultSubobjectClass<UCharacterMovementComponent>` **compiled, linked, and did nothing.**
Unreal refuses an override that does not DERIVE from the class the parent already set:

```
LogUObjectGlobals: Error: Class /Script/Engine.CharacterMovementComponent is not a legal override
for component CharMoveComp because it does not derive from ACFCharacterMovementComponent.
Will use ACFCharacterMovementComponent when constructing component.
```

One per character, and ACF's component was constructed anyway. **The build was green and the change
was inert** - which is the whole reason the log gets read instead of the diff.

Fix: `UGSCharacterMovementComponent : UACFCharacterMovementComponent`, which IS a legal override. Its
`BeginPlay` empties `LocomotionStates` (disarming `UpdateLocomotion`, whose loop cannot run on an
empty array), lets `Super::BeginPlay` run, then restores the authored `MaxWalkSpeed` - **the restore
is load-bearing**, because with the bands gone `Internal_ApplyLocomotionState(DefaultState)` resolves
to `0.0f` and would leave every character unable to move. Emptying rather than overriding because
`UpdateLocomotion`, `HandleStateChanged` and `Internal_ApplyLocomotionState` are all NON-virtual;
only `BeginPlay` and `TickComponent` are. `CharacterController` added to `Build.cs` - deriving from a
module's type requires linking it.

**Correction to Addendum 1:** it claimed `MaxWalkSpeed` was overwritten *every frame*. It is
overwritten on band TRANSITIONS. The sprint deadlock is unchanged; the wording was wrong.

## Addendum 3 - THE ACTUAL CAUSE: ACF turns actor tick off

Sprint was still dead after Addendum 2, and stamina turned out to be dead with it. That pairing is
what solved it - **both live in BP_GSPlayerCharacter's Event Tick**.

`ACFCharacter.cpp:83`:

```cpp
PrimaryActorTick.bStartWithTickEnabled = false;
```

Nothing in ACF ever turns it back on. The engine default is **true**, so this is ACF changing
behaviour out from under everything that derives from it. `AGSPlayerCharacter` sets
`bCanEverTick = true`, which was never the issue: CAN tick and STARTS ticking are different flags,
and our constructor runs after ACF's.

**Why it read as a movement bug for three rounds:** everything event-driven kept working perfectly -
attacks, dodge, interaction, blocking, death, the whole horde. Only the two tick-driven systems died,
and one of them is sprint, so the symptom presented as "sprint is broken" rather than "the actor is
not ticking".

**This is #135 repeating, with the roles reversed** - that ticket records a *subclass* constructor
silently disabling tick and costing two shipped features. This time the *base* class did it.

Fixed on `AGSCharacterBase` so every character gets it, not just the player.

### What the earlier addenda got wrong, recorded honestly

- **Addendum 1's fix was inert** and said so only after the log was read (Addendum 2).
- **Addendum 2's fix worked and was still not the bug.** PIE confirmed it: movement component
  `GSCharacterMovementComponent`, `MaxWalkSpeed` 470 at rest, `locomotion_states` 0. All correct, and
  sprint stayed broken.
- **The locomotion analysis was real but was not the cause.** ACF's state machine WOULD have made
  sprint impossible; it just was not what was making sprint impossible that night. Both fixes are
  kept: without the movement subclass, sprint breaks again the moment tick returns.

Three wrong diagnoses before asking what the symptom actually looked like. The question that ended it
was "does nothing happen, or does it happen weakly" - and the answer "nothing at all", plus stamina,
named the system in one step.
