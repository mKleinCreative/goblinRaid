---
name: gs-abilities-outside-acf-asc
description: Goblin Siege — GS abilities derive plain UGameplayAbility, so ACF's priority arbitration, input buffer, collision manager and montage lifetime are all idle underneath live combat.
globs: []
alwaysApply: false
---

# GS — our abilities stand outside ACF's ability system component

Every Goblin Siege character already owns ACF's ASC: `AACFCharacter`'s constructor creates it as
`ActionsComp` (literally named "Actions Manager", `ACFCharacter.cpp:70`), and
`AGSCharacterBase` caches `AbilitySystemComponent = Super::GetAbilitySystemComponent()`
(`GSCharacterBase.cpp:196`) — **that pointer IS ACF's `ActionsComp`** (the comment at
`GSCharacterBase.cpp:88` already records it). There is no `UACFActionsManagerComponent` in this build;
a grep across the plugin returns nothing. Actions **are** GAS abilities:
`UACFActionAbility : UACFGameplayAbility : UGameplayAbility` (`ActionsSystem/Public/Actions/ACFActionAbility.h:28`)
driven by `UACFAbilitySystemComponent : UAbilitySystemComponent`
(`ActionsSystem/Public/Components/ACFAbilitySystemComponent.h:47-59`).

So adopting ACF's action path is a **routing change, not a rewrite** — and the trap is the opposite of
the usual one: someone greps for "ActionsManagerComponent", finds nothing, and builds a parallel system
that is already there.

`actions-system` documents that priority arbitration and buffering exist and how to read them. It never
states the precondition that arms them, which is §1.

---

## 1 — The base class is what arms arbitration; ours does not

`CurrentPriority`, `bIsPerformingAction` and `PerformingAction` are written only from `OnAbilityStarted`
/ `OnAbilityEnded` (`ACFAbilitySystemComponent.cpp:359-370`), and those are called **exclusively** by
`UACFGameplayAbility::ActivateAbility` (`ACFGameplayAbility.cpp:147`) and `::EndAbility` (`:252`).
`OnAbilityEnded` is also what calls `EvaluateBuffer()` (`ACFAbilitySystemComponent.cpp:369`).

All eight GS combat abilities derive plain `UGameplayAbility`, not `UACFGameplayAbility`:
`GSGA_SwordLight.h:139`, `GSGA_Block.h:20`, `GSGA_BowShot.h:23`, `GSGA_DodgeRoll.h:13`,
`GSGA_GrappleThrow.h:20`, `GSGA_Horn.h:25`, `GSGA_Interact.h:14`, `GSGA_TorchToss.h:22` (all under
`Source/GoblinSiege/Weapons/Abilities/`). Combo state lives inside the ability instead —
`CurrentStage` / `bComboQueued` / `bBufferOpen` (`GSGA_SwordLight.cpp:230-231`, `:246`).

Result: `Internal_TriggerAction`'s gate — `(int32)Priority > CurrentPriority || Priority == EHighest`,
with buffering via `StoreAbilityInBuffer` (`ACFAbilitySystemComponent.cpp:133-176`) — is running against
`CurrentPriority` stuck at **-1** and `PerformingAction` null while a GS swing is live.

**The bite:** ACF's own code calls `TriggerAction` at `EHighest` on this same component —
`AACFCharacter::HandleDamageReceived` fires the hit-response action at `EActionPriority::EHighest`
(`ACFCharacter.cpp:356-360`, `:575`). That hit reaction passes the priority gate unconditionally and has
nothing to arbitrate against. Observable: a hit reaction and a sword swing running simultaneously, or
one silently eating the other, with no ordering rule anywhere.

---

## 2 — There is no AI combo buffer; the comment saying so is wrong

`AGSCharacterBase::TryLightAttack` (`GSCharacterBase.cpp:735-742`) claims:

> "GAS already refuses to re-activate an ability that is running, and `UGSGA_SwordLight` treats that
> refusal as the combo buffer - so an AI that spams this gets the same chained combo a player gets by
> mashing, for free."

`UGSGA_SwordLight::BufferComboInput()` (`GSGA_SwordLight.cpp:285`) has **exactly one call site in the
entire source tree**: `GSPlayerCharacter.cpp:842`. `UBTTask_MeleeAttack::ExecuteTask` calls only
`Self->TryLightAttack()` (`AI/Tasks/BTTask_MeleeAttack.cpp:259`), as does `UBTTask_SmashOrderTarget`
(`:63`). A refused `TryActivateAbilityByClass` returns false and does nothing else — **no flag is set.**
A refused activation is not a buffered input.

So every AI swing is stage 0, forever. Tuning `Damage` / `SweepArcDegrees` / `LungeSpeed` on
`FGSSwingStage` stages 2 and 3 is tuning data no defender or horde goblin will ever play, and the BT
cooldown gate (`BTTask_MeleeAttack.cpp:50`) hides it by pacing the swings anyway.

ACF has no such asymmetry because both sides enter through `TriggerAction`: AI via
`UACFCombatBehaviourComponent` (`ACFCombatBehaviourComponent.cpp:116`, `:148`, `:172`), player via
input, with buffering inside `Internal_TriggerAction` (`ACFAbilitySystemComponent.cpp:151-160`) and
combo advance in `UACFComboAction::SendComboInput` (`ACFComboAction.cpp:67-85`), driven by a gameplay
event the montage sends.

---

## 3 — An idle collision manager, and montages with no notifies

`AACFCharacter` constructs `CollisionComp = CreateDefaultSubobject<UACMCollisionManagerComponent>(...)`
on **every** GS character (`ACFCharacter.cpp:73`). ACF's damage window is authored on the montage:
`UACMActivateCollisionANS::NotifyBegin` calls `collisionComp->StartSingleTrace(trace)` per name in
`TracesToStart`, `NotifyEnd` calls `StopSingleTrace` (`ACMActivateCollisionANS.cpp:20-26`, `40-46`).
ACF also routes montage moments through the ASC as gameplay events — `UACFNotifyAction::Notify` fires
`Actions.AbilitySystem.NotablePoint` (`ACFNotifyAction.cpp:13-16`), with
`Actions.AbilitySystem.ApplyDamage` and `.ExitAction` alongside it (`ACFActionTypes.h:31-36`).

Goblin Siege does none of it. `UGSGA_SwordLight::OpenDamageWindow` calls `DoSweep()` then sets a
repeating `SweepTimer` at `SweepIntervalSeconds` and a `WindowTimer` to close it
(`GSGA_SwordLight.cpp:313-318`); `DoSweep` is `World->OverlapMultiByObjectType(...,
FCollisionShape::MakeSphere(S.SweepRadius), ...)` plus a manual arc test (`:336-368`). Timings are CDO
fields on `FGSSwingStage` (`WindupSeconds`, `DamageWindowSeconds`, `RecoverySeconds`,
`GSGA_SwordLight.h`). The GS source tree has **zero** references to `CollisionComp`,
`UACMCollisionManagerComponent`, `StartSingleTrace` or `ACMActivateCollisionANS`.

And the montages carry no notifies at all: `grep -a -o 'AnimNotify_[A-Za-z_]+|UACF[A-Za-z]+|ACM[A-Za-z]+'`
over `Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Light.uasset` and `AM_HU_Atk_Heavy.uasset`
returns **nothing**.

Two consequences:

- **Configuring a weapon's `DamageTraces` produces silence.** `collisions-manager:159` will point you at
  the montage — correctly — without telling you that *every* GS attack montage is empty, and that
  structural montage edits are hazardous in this editor.
- **`GSGA_SwordLight.h:9-12` says moving the window onto notifies "is mechanical". It is not:** it means
  authoring ACM trace configurations plus an ANS on every attack montage, per skeleton (human and goblin
  have separate assets). Meanwhile a `UACMCollisionManagerComponent` ticks on every character in the
  siege and hits nothing.

---

## 4 — Fire-and-forget montages: a refused montage still deals full damage

`UGSGA_SwordLight::RunStage` plays with `Char->PlayAnimMontage(S.Montage, S.MontagePlayRate)` and
**discards the return** (`GSGA_SwordLight.cpp:258`). `AGSCharacterBase::PlayAnimMontage` returns `0.f`
without playing anything on a skeleton mismatch, logging only under the `GS.LogHitReact` cvar
(`GSCharacterBase.cpp:493-514`). `AM_HU_Atk_Light.uasset` declares `SlotName` "UpperBody".

Because the swing sequence is **timers**, not the montage, a refused montage still runs its full damage
window: an invisible hit that nothing downstream can notice. Contrast ACF, where the action's lifetime
*is* the montage's: `UACFGameplayAbility::PlayCurrentMontage` uses `UAbilityTask_PlayMontageAndWait` and
binds OnBlendOut / OnInterrupted / OnCancelled / OnCompleted (`ACFGameplayAbility.cpp:262-273`).

### The second half, if you migrate to `UACFAttackAction`

`UACFAttackAction`'s constructor sets `ActionConfig.MontageReproductionType = EMotionWarped`
(`ACFAttackAction.cpp:25`). `SetupAttack` warps toward the target only if a `UMotionWarpingComponent`
**and** a `UATSBaseTargetComponent` on the controller **and** a non-null `targetComp->GetCurrentTarget()`
are all present; otherwise it silently downgrades to `ERootMotion` (`ACFAttackAction.cpp:38-75`).

`AGSAIControllerBase` inherits a `UATSAITargetComponent` from `AACFAIController`
(`ACFAIController.cpp:56`), but `UBTService_AcquireTarget` only writes the blackboard key
(`BTService_AcquireTarget.cpp:282`) — ACF's targeting component is **never told who the target is**
(see `gs-behaviour-tree-wiring` §4). So the warp silently downgrades to root motion, which an
UpperBody-slot montage does not supply either (see `gs-anim-adoption-gaps` §1). Symptom: "ACF's attack
plays in place and never reaches" — cause: a blackboard key written instead of a component.

GS closes distance with `Char->LaunchCharacter(Char->GetActorForwardVector() * S.LungeSpeed, true, false)`
(`GSGA_SwordLight.cpp:208`); there is no reference to MotionWarp anywhere in GS source, though
`AACFCharacter` builds a `UMotionWarpingComponent` for every character (`ACFCharacter.cpp:78`).
