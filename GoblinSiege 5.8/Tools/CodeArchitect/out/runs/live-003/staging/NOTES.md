# Interact framework — hold-E channels + carry — generated live-003

> **Revised 2026-08-04 after review**, per Michael's four rulings. Changes to the generated code:
> 1. **Focused interactable wins over put-down** (`ResolveChannelTarget`) — full hands can still
>    loot and foul a well. A second *carryable* is still refused while carrying (one slot for the
>    slice), because its channel would consume the object and then fail to pick it up.
>    *(Extraction, the case that originally motivated this, is an auto-bank circle as of the
>    2026-08-04 ruling — the rule still stands for the other verbs.)*
> 2. **Carry stays one slot, attacks blocked.** GDD §9's "chickens weightless (carry two, fight
>    one-handed)" needs weight classes — deferred to the livestock pass.
> 3. **The movement slow is now a GameplayEffect on `MoveSpeedMultiplier`** (new
>    `Combat/GSGE_MoveSpeedScalar`), not cache-and-restore on `MaxWalkSpeed`. `GSGA_Block` converts
>    to it too — see patch 6. This is what fixes crouch silently cancelling the carry slow.
> 4. **Server authority is in.** Server RPCs for begin/abort, payout and `SetAvailable` behind
>    `HasAuthority`, `bIsInteracting` replicated `COND_SkipOwner`. The ability stays `LocalOnly` and
>    nothing is predicted. On a listen server — every single-player PIE session — no RPC is sent and
>    the behaviour is identical to before.
>
> The patch list below has been updated to match: one new tag, a new speed-derivation patch on
> `AGSPlayerCharacter`, and a new patch converting `UGSGA_Block` off its cached `MaxWalkSpeed`.

## Patches to existing files

### 1. `Source/GoblinSiege/Combat/GSGameplayTags.h`

Insert immediately **after** the `UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardBroken);` line and before the `// ---- burn objective types` banner:

```cpp
	/** Hold-E channel in progress (GDD §8). Owned by UGSGA_Interact's ActivationOwnedTags, so GAS
	 *  adds and removes it for exactly the channel's lifetime - nothing else should set it. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Interacting);

	/** Hands full. Applied as a loose tag by UGSCarryComponent for exactly as long as the object is
	 *  held; attack abilities block on it (that is the "cannot swing while carrying" rule). */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Carrying);

	// ------------------------------------------------------------------ interaction verbs
	// The slice's channelled verbs plus the carry pick-up/put-down channel. These are DATA: an
	// interactable advertises one through UGSInteractableComponent::VerbTag, no C++ branches on
	// them, and a fifth verb costs a tag rather than a subclass.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Loot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Takedown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_FoulWell);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Carry);

	/** RESERVED, deliberately unused as of 2026-08-04: extraction is an auto-bank circle (GDD §9),
	 *  not a hold-E channel. Kept declared because §12.1 lists extract among the slice verbs and the
	 *  ruling was "for now" - if it becomes channelled, this is the tag and nothing else changes. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Extract);

	// ------------------------------------------------------------------ SetByCaller data keys
	/** Magnitude key for UGSGE_MoveSpeedScalar. Same convention as the Damage.* tags, which double
	 *  as SetByCaller keys on the damage spec: one effect class, many callers, no GE per source. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_MoveSpeedScalar);
```

**Required** — `UGSGE_MoveSpeedScalar`, `UGSCarryComponent` and (after patch 6) `UGSGA_Block` all
reference `GSTags::Data_MoveSpeedScalar`. Without this line none of the three compile.

### 2. `Source/GoblinSiege/Combat/GSGameplayTags.cpp`

Add inside `namespace GSTags { ... }`, after the `State_GuardBroken` definition. **Match the macro form already used in that file** — if the existing lines use `UE_DEFINE_GAMEPLAY_TAG` without a comment string, drop the third argument:

```cpp
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Interacting, "State.Interacting", "Hold-E channel in progress.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Carrying, "State.Carrying", "Carrying an object - slower, and cannot attack.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Loot, "Interact.Loot", "Loot a container or a corpse.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Takedown, "Interact.Takedown", "Stealth takedown on an unaware defender.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_FoulWell, "Interact.FoulWell", "Foul a village well.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Extract, "Interact.Extract", "RESERVED - extraction auto-banks on a circle for now, not a channel.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Carry, "Interact.Carry", "Pick up / put down a carryable object.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_MoveSpeedScalar, "Data.MoveSpeedScalar", "SetByCaller key: multiplier fed to UGSGE_MoveSpeedScalar.");
```

### 3. `Source/GoblinSiege/Characters/GSPlayerCharacter.h`

**(a)** Add to the forward declarations at the top (beside `class UGSWeaponComponent;`):

```cpp
class UGSInteractionComponent;
class UGSCarryComponent;
```

**(b)** In the `public:` accessor block, after `GetTargetingComponent()`:

```cpp
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Interaction")
	UGSCarryComponent* GetCarryComponent() const { return CarryComponent; }
```

**(c)** In `protected:`, after the `Input_ToggleCrouch` declaration:

```cpp
	/** Hold E to channel, release to abort. Release is routed straight at the interaction component
	 *  because the ability is activated by class rather than through an ASC input ID, so GAS's own
	 *  InputReleased never fires for it. */
	void Input_InteractStart(const FInputActionValue& Value);
	void Input_InteractStop(const FInputActionValue& Value);
```

**(d)** After the `TargetingComponent` UPROPERTY:

```cpp
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Interaction")
	TObjectPtr<UGSCarryComponent> CarryComponent;
```

**(e)** After the `CrouchAction` UPROPERTY:

```cpp
	/** Hold-E interact (GDD §8). Bound to Started and Completed/Canceled - the channel runs exactly
	 *  as long as the key is down. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Input")
	TObjectPtr<UInputAction> InteractAction;
```

**(f)** After the `GuardBreakAbilityClass` UPROPERTY:

```cpp
	/** Universal interact channel - C++-defaulted to UGSGA_Interact, same reasoning as
	 *  SwordLightAbilityClass: a framework verb nobody remembers to fill in is a framework nobody
	 *  can test. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Abilities")
	TSubclassOf<UGameplayAbility> InteractAbilityClass;
```

**(g)** In `protected:`, beside the other handlers. `struct FOnAttributeChangeData;` is already
forward-declared in `GSCharacterBase.h`, which this header includes:

```cpp
	/** MoveSpeedMultiplier is the single source of truth for walk speed: the carry slow, the block
	 *  slow, and every root/slow after them are GameplayEffects on that attribute, aggregated by GAS.
	 *  Nothing writes MaxWalkSpeed directly any more - that pattern cannot stack, and OnStartCrouch
	 *  reassigned MaxWalkSpeedCrouched out from under whoever had cached it. */
	void HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);

	/** Re-derives both walk speeds from BaseWalkSpeed and the current multiplier. */
	void ApplyMoveSpeed();
```

### 4. `Source/GoblinSiege/Characters/GSPlayerCharacter.cpp`

**(a)** Add to the include block:

```cpp
#include "Interaction/GSCarryComponent.h"
#include "Interaction/GSInteractionComponent.h"
#include "Weapons/Abilities/GSGA_Interact.h"
```

**(b)** In `AGSPlayerCharacter::AGSPlayerCharacter()`, beside the existing `CreateDefaultSubobject` calls:

```cpp
	InteractionComponent = CreateDefaultSubobject<UGSInteractionComponent>(TEXT("InteractionComponent"));
	CarryComponent = CreateDefaultSubobject<UGSCarryComponent>(TEXT("CarryComponent"));
	InteractAbilityClass = UGSGA_Interact::StaticClass();
```

**(c)** In `BeginPlay()`, alongside the existing torch-toss / dodge grants — copy the shape of whichever grant block is already there (authority check + `GiveAbility`), e.g.:

```cpp
	if (AbilitySystemComponent && HasAuthority() && InteractAbilityClass)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(InteractAbilityClass, 1, INDEX_NONE, this));
	}
```

**(d)** In `SetupPlayerInputComponent()`, with the other `BindAction` calls (use whatever the local variable for the Enhanced Input component is named):

```cpp
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AGSPlayerCharacter::Input_InteractStart);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &AGSPlayerCharacter::Input_InteractStop);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AGSPlayerCharacter::Input_InteractStop);
	}
```

**(e)** Add the two handlers, beside `Input_ToggleCrouch`:

```cpp
void AGSPlayerCharacter::Input_InteractStart(const FInputActionValue& /*Value*/)
{
	if (AbilitySystemComponent && InteractAbilityClass)
	{
		AbilitySystemComponent->TryActivateAbilityByClass(InteractAbilityClass);
	}
}

void AGSPlayerCharacter::Input_InteractStop(const FInputActionValue& /*Value*/)
{
	// Interruptible always (stealth spec): letting go cancels, including on an alt-tab (Canceled).
	if (InteractionComponent)
	{
		InteractionComponent->ReleaseInteractInput();
	}
}
```

**(f)** The speed derivation. Add these includes:

```cpp
#include "Attributes/GSAttributeSetBase.h"
#include "GameplayEffectExtension.h"   // FOnAttributeChangeData
```

In `BeginPlay()`, **after** the existing `BaseWalkSpeed = MoveComp->MaxWalkSpeed;` block (it has to
capture the unmodified value first):

```cpp
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute())
			.AddUObject(this, &AGSPlayerCharacter::HandleMoveSpeedMultiplierChanged);
	}
	ApplyMoveSpeed();
```

Add the two functions:

```cpp
void AGSPlayerCharacter::HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& /*Data*/)
{
	ApplyMoveSpeed();
}

void AGSPlayerCharacter::ApplyMoveSpeed()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !AbilitySystemComponent)
	{
		return;
	}

	// UGSAttributeSetBase::PreAttributeChange already clamps this to [0.1, 3.0], so a stack of slows
	// can never hard-freeze the goblin.
	const float Multiplier = AbilitySystemComponent->GetNumericAttribute(
		UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute());

	MoveComp->MaxWalkSpeed = BaseWalkSpeed * Multiplier;
	MoveComp->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier * Multiplier;
}
```

**(g)** Replace the body of the movement block in `OnStartCrouch` — this line is the crouch bug:

```cpp
	// BEFORE (drops any active slow on the floor):
	//   MoveComp->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier;
	ApplyMoveSpeed();
```

Do the same in `OnEndCrouch` if it writes either speed. Both then respect whatever slows are active
instead of racing them.

### 5. Attack abilities — block while carrying

In the constructor of `UGSGA_SwordLight` (`Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp`) and `UGSGA_Block` (`.../GSGA_Block.cpp`), beside the existing `ActivationBlockedTags` lines, add:

```cpp
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
	ActivationBlockedTags.AddTag(GSTags::State_Interacting);
```

**Required, not optional** (ruling 2026-08-04: *"a full handed goblin has to drop what they're holding
before they can throw a torch"*) — the same line in `UGSGA_TorchToss`'s constructor
(`Source/GoblinSiege/Weapons/Abilities/GSGA_TorchToss.cpp`):

```cpp
	ActivationBlockedTags.AddTag(GSTags::State_Carrying);
```

Note the tag only blocks *activation* — a swing already in flight when you pick something up
finishes; if that reads badly in playtest, call `ASC->CancelAbilities()` from
`UGSCarryComponent::ApplyCarryState()`.

`UGSGA_Block` already blocks on `State.GuardBroken` (`GSGA_Block.cpp:33`), so the "cannot block while
staggered" half of that ruling needs no change. The interact half is in the staged
`GSGA_Interact.cpp` constructor.

### 5b. `UGSGA_SwordLight::BreakGuard` — let the kick cancel a channel too

Staggered hands cannot loot, and `ActivationBlockedTags` only refuses a *start*. A goblin who was
already channelling when the kick landed would keep looting through the stagger. `BreakGuard` already
cancels by tag, so this is one line — in
`Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp`, in the container built around line 324:

```cpp
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(GSTags::State_Blocking);
	BlockTags.AddTag(GSTags::State_Interacting);   // ADD: a broken guard also rips open a channel
	TargetASC->CancelAbilities(&BlockTags);
```

Consider renaming the local to `CancelTags` — it is no longer only about the guard.

This only ever fires against a target that already has `State.Blocking` (`BreakGuard` returns early
otherwise), so the case it covers is precisely: guard up, channel started, kick lands. Cancelling
`UGSGA_Interact` runs its `EndAbility`, which already aborts the channel on external cancellation.

### 6. `UGSGA_Block` — convert off the cached `MaxWalkSpeed`

Both slows must go through the same attribute or they clobber each other. `GSGA_Block` is
`ServerInitiated`, so it is already running on the authority when it applies the effect.

In `Source/GoblinSiege/Weapons/Abilities/GSGA_Block.h`, replace the cached float:

```cpp
	// REMOVE: float CachedMaxWalkSpeed = 0.f;
	FActiveGameplayEffectHandle BlockSlowHandle;
```

with `#include "ActiveGameplayEffectHandle.h"` at the top, and keep `BlockMoveSpeedScale` as it is.

In `GSGA_Block.cpp`, add `#include "Combat/GSGE_MoveSpeedScalar.h"` and replace the movement block in
`ActivateAbility`:

```cpp
	// BEFORE:
	//   if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	//   {
	//       CachedMaxWalkSpeed = Move->MaxWalkSpeed;
	//       Move->MaxWalkSpeed = CachedMaxWalkSpeed * BlockMoveSpeedScale;
	//   }
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			UGSGE_MoveSpeedScalar::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(GSTags::Data_MoveSpeedScalar, BlockMoveSpeedScale);
			BlockSlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
```

and in `EndAbility`:

```cpp
	// BEFORE: restore CachedMaxWalkSpeed
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (BlockSlowHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(BlockSlowHandle);
		}
	}
	BlockSlowHandle = FActiveGameplayEffectHandle();
```

The existing comment on the old restore ("Restore rather than recompute: whatever the speed was when
the guard went up is what it should be when the guard comes down, including any buff or slow that was
already applied") describes what the attribute now does correctly and unconditionally — its intent
survives, its mechanism does not. Worth rewriting rather than deleting.

### 7. `Source/GoblinSiege/GoblinSiege.Build.cs`

No change required — the new files use only `Core`, `CoreUObject`, `Engine`, `GameplayAbilities`, `GameplayTags` and `NetCore`, all of which the module already needs for GAS. **Verify** `PublicDependencyModuleNames` contains `"GameplayAbilities"`, `"GameplayTags"`, `"GameplayTasks"`; add any that are missing (they should already be there for `UGSGA_SwordLight`).

### 8. Editor-side wiring (not C++)

- **`IA_Interact`** (Digital / bool) under `Content/Input/`, mapped to **E** in `IMC_Default`, then assigned to `InteractAction` on `BP_GSPlayerCharacter`.
- **`CarrySocket`** on `GOB_Scout_v2_Skeleton` — a socket on the chest or right hand. Without it the carried object snaps to the mesh origin (feet), which looks wrong but does not break anything.
- **HUD**: `WBP_GSPlayerHUD` binds to the interaction component's `OnChannelStarted` (show the bar, print the verb), `OnChannelProgress` (0..1 → progress bar percent), `OnChannelEnded` (hide; `Reason` is available if you want "interrupted" feedback), and `OnFocusChanged` (world prompt using `GetPromptText()`).
- **Test props**: add `UGSInteractableComponent` to a Blueprint actor, set `VerbTag = Interact.Loot`, `ChannelSeconds = 1.5`. For carry, a second actor with `bIsCarryable = true`, `VerbTag = Interact.Carry`, `bConsumeOnComplete = false`.
- **Debug**: `GS.Interact.Debug 1` draws the range sphere, the facing axis, the focused interactable and the live channel percentage — the same cvar shape as `GS.Combat.Debug`.

### Build note

Five new `UCLASS` types (three components, one ability, one GameplayEffect), a new `UENUM`, and new
native gameplay tags — **Live Coding cannot register any of these**. Close the editor and run the
full build:

```powershell
& "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" --% MyProjectEditor Win64 Development -Project="D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject" -WaitMutex
```


- Verbs are gameplay tags on the interactable, never subclasses (GDD §4). Nothing in C++ branches on the verb — the framework channels, then calls the interactable's completion hook. A fifth verb (foul the granary, cut the rope bridge) is a tag plus a Blueprint, not a compile.
- The channel state machine lives on UGSInteractionComponent, not on UGSGA_Interact. The HUD bar has to bind to something that exists between channels, and abort rules that depend on range/facing want a ticking component anyway. The ability keeps what only GAS can do: activation gating, the State.Interacting tag, and cancellation.
- Damage aborts by binding to the existing AGSCharacterBase::OnHealthChanged (negative delta) rather than introducing a gameplay-event route. That delegate already fires on every write with a signed delta, and it works for damage from any source — an event tag would only catch the sources someone remembered to tag.
- Range and facing are the interactor's job; availability and contention are the interactable's. Splitting the eligibility test this way means a chest never needs to know a cone angle, and the cone can be tuned once on the SCOUT instead of on every prop.
- Facing uses control rotation when a controller exists. The pawn may still be turning toward the camera under tech doc §16's rotation modes, so measuring the cone off the mesh's current yaw would refuse interactions the player is plainly aiming at. Abort checks carry range and facing slack (60cm, 0.15 dot) so a footstep at the boundary does not cancel a channel mid-loot.
- Zero-second verbs still route through BeginChannel → CompleteChannel rather than short-circuiting, so there is exactly one path to the payout. That is why UGSGA_Interact binds OnChannelEnded before calling BeginChannel — an instant verb finishes inside the activation call.
- The carry hand-off happens in UGSInteractionComponent::CompleteChannel, not inside the interactable's completion event. A crate should not know what a carrier is; the component is the only party that already holds both ends.
- Put-down reuses the channel with a null interactable and Interact.Carry, so it inherits every abort rule and the same HUD bar for free instead of being a second, subtly different code path.
- State.Carrying is a loose tag, not a GameplayEffect: its lifetime is exactly "while the object is held", which is object state, not a duration anyone can author. Movement slow scales MaxWalkSpeed/MaxWalkSpeedCrouched directly and restores the cached values — the MoveSpeedMultiplier attribute is not currently wired into CharacterMovement, so using it would slow nothing.
- Multiplayer posture honoured: bIsAvailable, bIsInteracting and CarriedActor replicate as cheap root state; progress, focus, and the abort checks are local, and the ability is LocalOnly with no prediction. The interactable also holds a one-at-a-time channel lock so two goblins cannot loot the same chest.
- UGSGA_Interact sits in Weapons/Abilities beside GSGA_DodgeRoll and GSGA_TorchToss — that folder is where every UGameplayAbility in the project lives, and interact is the same kind of universal racial verb those two are. The three components live in a new Interaction/ folder since they are the framework itself.
- UGSCarryComponent drops on the owner's death (bound to OnDied) — with ragdoll death now adopted, a sack welded to a tumbling corpse would be the first thing anyone notices in a playtest.