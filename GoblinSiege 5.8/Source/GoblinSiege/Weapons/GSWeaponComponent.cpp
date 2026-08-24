#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Characters/GSCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h" // GetSkinnedAsset() in the missing-socket log line names the mesh
#include "GameFramework/Character.h"
#include "TimerManager.h"

namespace
{
	/** Log-only. Not UEnum::GetDisplayValueAsText: this runs on every slot change and the reflection
	 *  lookup is not worth it for a line that says "is now holding Sword". */
	FString SlotName(EGSWeaponSlot Slot)
	{
		switch (Slot)
		{
		case EGSWeaponSlot::Torch:   return TEXT("Torch");
		case EGSWeaponSlot::Bow:     return TEXT("Bow");
		case EGSWeaponSlot::Grapple: return TEXT("Grapple");
		default:                     return TEXT("Sword");
		}
	}
}

UGSWeaponComponent::UGSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// Apply the Blueprint-assigned starting kit through the same path as a runtime swap.
	if (EquippedWeapon)
	{
		UGSWeaponDataAsset* Initial = EquippedWeapon;
		EquippedWeapon = nullptr;
		EquipWeapon(Initial);
	}
	else
	{
		// 2026-08-02: this was the only failure in this component with NO diagnostic at all - every
		// mesh/socket problem warns, but a null data asset just silently skipped the whole equip.
		// It costs an afternoon because the symptom (no weapon, no abilities, default attributes)
		// looks exactly like a broken socket or a bad mesh path. It is also the single most likely
		// state for a freshly-made character Blueprint, which is why it is a Warning and not a Log.
		if (bExpectsExternalEquip)
		{
			// Not a defect: this owner arms itself a moment from now. Still SAID, at Verbose, so
			// that a genuinely broken external equip is not silent either.
			UE_LOG(LogTemp, Verbose,
				TEXT("[GoblinSiege] %s starts with no EquippedWeapon and expects its owner to equip "
					 "it - this is normal for the horde."), *GetNameSafe(GetOwner()));
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GoblinSiege] %s has no EquippedWeapon data asset - no weapon meshes, no granted "
					 "abilities, and no InitialAttributesEffect, so this character keeps the attribute "
					 "set's constructor defaults (100/100/0). Assign one on the owning Blueprint."),
				*GetNameSafe(GetOwner()));
		}
	}
}

void UGSWeaponComponent::SetAimActive(bool bInAimActive)
{
	if (bAimActive == bInAimActive)
	{
		return;
	}

	bAimActive = bInAimActive;

	// Placement only - RebuildWeaponMeshes would tear down and recreate the components mid-aim.
	RefreshWeaponMeshPlacement();
}

void UGSWeaponComponent::RefreshWeaponVisuals()
{
	// 2026-08-02. The header has long promised that weapon offsets can be tuned by re-equipping,
	// but RebuildWeaponMeshes is protected and non-UFUNCTION, so in practice the only ways to see an
	// offset change were a PIE restart or a weapon-mode toggle. Feel-tuning a sword's placement is a
	// twenty-iteration job; at ~3 minutes of editor start per iteration that is an afternoon.
	// CallInEditor puts a button on the details panel instead.
	if (!EquippedWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] RefreshWeaponVisuals: no EquippedWeapon to refresh."));
		return;
	}

	RebuildWeaponMeshes();
}

void UGSWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Timer first: the swap lock's weak lambda is already safe against a destroyed component, but
	// every other EndPlay in this module clears its handles here and consistency is worth more than
	// the one line it costs (see AGSBurnObjectiveBase, UGSFlammableComponent, UGSBuffAuraComponent).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapLockTimerHandle);
	}

	// Nothing this component created may outlive it. These were registered with the owning actor at
	// runtime, so they will not be cleaned up by the actor's own construction-time teardown.
	DestroyWeaponMeshes();

	Super::EndPlay(EndPlayReason);
}

void UGSWeaponComponent::EquipWeapon(UGSWeaponDataAsset* NewWeapon)
{
	if (!NewWeapon || NewWeapon == EquippedWeapon)
	{
		return;
	}

	EquippedWeapon = NewWeapon;
	// Straight assignment, not SetSlot: this is an equip, not a swap. SetSlot would refuse under the
	// anti-cancel lock, broadcast a slot change nobody is listening for yet, and ready a torch prop
	// before the meshes below have been built.
	CurrentSlot = EGSWeaponSlot::Sword;

	// A new weapon gets a clean slate on both warn latches - it names its own asset paths and its
	// own sockets, and the previous kit's failures say nothing about this one. See the header.
	bMeleeMeshResolveFailed = false;
	bRangedMeshResolveFailed = false;
	bQuiverMeshResolveFailed = false;
	bHeldTorchMeshResolveFailed = false;
	WarnedMissingSockets.Reset();

	// Stats and abilities FIRST, visuals second, and that order is deliberate: the mesh work below
	// touches the skeletal mesh, the socket table and the asset registry, all of which are far more
	// likely to be missing or half-authored than a GameplayEffect class is. If the visual half ever
	// throws, the weapon has already applied its turn rate, its attribute baseline and its
	// abilities - the goblin fights correctly with invisible gear rather than not fighting at all.
	GrantAbilitiesFromWeapon();

	// Cosmetics run on EVERY machine, unlike GrantAbilitiesFromWeapon which authority-gates itself:
	// a client that can't see its own sword has no game. (EquippedWeapon is not replicated today,
	// so a client builds its meshes from the Blueprint-assigned default in BeginPlay; when a
	// runtime swap needs to be seen by other clients, replicating this pointer with an OnRep that
	// calls RebuildWeaponMeshes is the whole of that change.)
	RebuildWeaponMeshes();
}

void UGSWeaponComponent::GrantAbilitiesFromWeapon()
{
	AGSCharacterBase* OwnerCharacter = Cast<AGSCharacterBase>(GetOwner());
	UAbilitySystemComponent* ASC = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !EquippedWeapon || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// Per-weapon turn-rate identity (Brute turns like a barge, Scout/Slasher turns sharp - design
	// doc "Turn rate"). Lives on the character (a movement concern), pushed here since equip is
	// the one moment a weapon's identity applies itself to its wearer (tech doc §16).
	OwnerCharacter->SetTurnRateRadPerSec(EquippedWeapon->TurnRateRadPerSec);

	// Class baseline stats ride on the weapon's init effect (design doc: class == weapon).
	if (EquippedWeapon->InitialAttributesEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(EquippedWeapon);
		FGameplayEffectSpecHandle SpecHandle =
			ASC->MakeOutgoingSpec(EquippedWeapon->InitialAttributesEffect, 1.f, Context);
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : EquippedWeapon->GrantedAbilities)
	{
		if (AbilityClass)
		{
			ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, GetOwner()));
		}
	}
	// Revoking the previous kit's abilities on swap (ClearAbility on stored handles) is left for
	// the pass that adds a second equippable kit - the slice equips one kit per raid.
}

void UGSWeaponComponent::ToggleRangedMode()
{
	// Split out of one silent early-out on 2026-08-05. Three completely different causes used to
	// share this line and produce the identical symptom - "I pressed swap and nothing happened" -
	// with nothing in the log to tell them apart. Two of the three are misconfigurations a person
	// has to go and fix, so they say so and name the field.
	if (bSwapLocked)
	{
		// Not a misconfiguration: the anti-cancel lock doing exactly its job on a double tap.
		// Log, not Warning - this one is supposed to happen.
		UE_LOG(LogTemp, Log,
			TEXT("[GoblinSiege] Weapon swap ignored on %s - the %.2fs anti-cancel lock is still up. "
				 "Normal on a fast double tap."),
			*GetNameSafe(GetOwner()), SwapInputLockSeconds);
		return;
	}
	if (!EquippedWeapon)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Weapon swap refused on %s - no weapon equipped. Set EquippedWeapon "
				 "on the character Blueprint's WeaponComponent."),
			*GetNameSafe(GetOwner()));
		return;
	}
	if (!EquippedWeapon->bHasRangedMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Weapon swap refused on %s - '%s' has bHasRangedMode unticked, so it "
				 "has no ranged half to swap to. Tick it under GoblinSiege|Weapon|RangedMode on the "
				 "data asset; it is data, so no recompile is needed."),
			*GetNameSafe(GetOwner()), *GetNameSafe(EquippedWeapon));
		return;
	}

	// The legacy two-way swap, now expressed as a slot change. Torch is deliberately NOT in this
	// cycle: this is the sword<->bow key, and a player who has never opened the wheel should not
	// find a torch in their hand because they tapped swap twice.
	SetSlot(CurrentSlot == EGSWeaponSlot::Bow ? EGSWeaponSlot::Sword : EGSWeaponSlot::Bow);
}

bool UGSWeaponComponent::SetSlot(EGSWeaponSlot NewSlot)
{
	if (NewSlot == CurrentSlot)
	{
		return false;
	}

	// Same three refusals ToggleRangedMode split out on 2026-08-05, each naming its own cause,
	// because "I pressed swap and nothing happened" with one silent early-out cost real time.
	if (bSwapLocked)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[GoblinSiege] Weapon slot change ignored on %s - the %.2fs anti-cancel lock is "
				 "still up. Normal on a fast double tap."),
			*GetNameSafe(GetOwner()), SwapInputLockSeconds);
		return false;
	}
	if (!EquippedWeapon)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Weapon slot change refused on %s - no weapon equipped. Set "
				 "EquippedWeapon on the character Blueprint's WeaponComponent."),
			*GetNameSafe(GetOwner()));
		return false;
	}
	if (NewSlot == EGSWeaponSlot::Bow && !EquippedWeapon->bHasRangedMode)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Bow slot refused on %s - '%s' has bHasRangedMode unticked, so it has "
				 "no ranged half. Tick it under GoblinSiege|Weapon|RangedMode on the data asset; it "
				 "is data, so no recompile is needed."),
			*GetNameSafe(GetOwner()), *GetNameSafe(EquippedWeapon));
		return false;
	}

	CurrentSlot = NewSlot;

	// Hands first, broadcasts second - so anything listening (AGSPlayerCharacter's rotation mode
	// today, a HUD or an AnimBP tomorrow) sees a character whose hands already agree with the state
	// it is being told about. No component is created or destroyed for sword/bow; both halves exist
	// and only change socket, which is why the swap stays cheap enough to spam.
	RefreshWeaponMeshPlacement();

	// The torch is a HELD weapon now, not a 0.25s loan during an ability (Michael, 2026-08-06).
	// UGSGA_TorchToss still readies and un-readies around its own throw; making the slot own the
	// prop is what stops the torch vanishing from the hand the instant the projectile spawns.
	SetTorchReadied(CurrentSlot == EGSWeaponSlot::Torch);

	OnWeaponSlotChanged.Broadcast(CurrentSlot);
	OnWeaponModeChanged.Broadcast(IsInRangedMode());

	UE_LOG(LogTemp, Log, TEXT("[GoblinSiege] %s is now holding %s."),
		*GetNameSafe(GetOwner()), *SlotName(CurrentSlot));

	bSwapLocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SwapLockTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]() { bSwapLocked = false; }),
			SwapInputLockSeconds, false);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Radial wheel. See the header for why this is delta-based and why the maths lives here.
// ---------------------------------------------------------------------------------------------

EGSWeaponSlot UGSWeaponComponent::SlotForDirection(FVector2D Direction, EGSWeaponSlot FallbackSlot) const
{
	if (Direction.Size() < WheelDeadZone)
	{
		return FallbackSlot;
	}

	// Screen space has +Y DOWN, so negate it to get a conventional maths angle where up is +90.
	// Getting this backwards silently mirrors the wheel top-to-bottom, which is the kind of bug
	// that reads as "the sectors feel wrong" rather than as an error.
	const float Degrees = FRotator::ClampAxis(
		FMath::RadiansToDegrees(FMath::Atan2(-Direction.Y, Direction.X)));

	// 90-degree sectors centred on 90 (top), 180 (left), 270 (bottom) and 0 (right).
	// Bow and Sword each shifted 30 degrees when the fourth slot landed, rather than rotating the
	// whole wheel: a player reaching left for the sword still finds it left.
	if (Degrees >= 45.f  && Degrees < 135.f) { return EGSWeaponSlot::Torch; }
	if (Degrees >= 135.f && Degrees < 225.f) { return EGSWeaponSlot::Sword; }
	if (Degrees >= 225.f && Degrees < 315.f) { return EGSWeaponSlot::Grapple; }
	return EGSWeaponSlot::Bow;
}

void UGSWeaponComponent::OpenWeaponWheel()
{
	if (bWheelOpen)
	{
		return;
	}
	bWheelOpen = true;
	WheelAccum = FVector2D::ZeroVector;

	// Starts on what is already held, so a Q tap with no drag commits the slot you are in - i.e.
	// nothing happens. Starting on Torch (enum 0) would arm the player for pressing a key.
	WheelHighlight = CurrentSlot;

	OnWheelOpenChanged.Broadcast(true);
	OnWheelHighlightChanged.Broadcast(WheelHighlight);
}

void UGSWeaponComponent::AddWheelInput(FVector2D Delta)
{
	if (!bWheelOpen)
	{
		return;
	}
	WheelAccum += Delta;

	const EGSWeaponSlot NewHighlight = SlotForDirection(WheelAccum, CurrentSlot);
	if (NewHighlight != WheelHighlight)
	{
		WheelHighlight = NewHighlight;
		// On change only. A per-frame broadcast would have the widget rebuilding itself 120 times a
		// second to draw the same thing.
		OnWheelHighlightChanged.Broadcast(WheelHighlight);
	}
}

void UGSWeaponComponent::CloseWeaponWheel(bool bCommit)
{
	if (!bWheelOpen)
	{
		return;
	}
	bWheelOpen = false;

	// Inside the dead zone the highlight still equals CurrentSlot, so "drag back to centre cancels"
	// needs no special case - SetSlot returns false on a no-op change. The explicit bCommit exists
	// for the caller that knows the gesture was aborted (menu opened, pawn died) without the player
	// having dragged back.
	const EGSWeaponSlot Chosen = bCommit ? WheelHighlight : CurrentSlot;

	WheelAccum = FVector2D::ZeroVector;
	OnWheelOpenChanged.Broadcast(false);

	SetSlot(Chosen);
}

// ---------------------------------------------------------------------------------------------
// Visible weapons (2026-08-01). See the header for why these are static meshes and why the
// missing-socket path is the one that actually runs today.
// ---------------------------------------------------------------------------------------------

UStaticMeshComponent* UGSWeaponComponent::GetActiveWeaponMesh() const
{
	// "Active" means in hand, not "exists": the holstered half is still a live component sitting on
	// the goblin's back, and a hit trace must never pick it up.
	//
	// The Torch slot returns the MELEE mesh, not null and not the torch. Holding a torch does not
	// arm you with one - the header is explicit that a sword ability tracing along a torch would be
	// a bug nobody would find - and callers already treat null as "no art yet", so returning null
	// here would silently mean something else. GetHeldTorchMesh() is the torch's accessor.
	const bool bRangedActive = IsInRangedMode() && EquippedWeapon && EquippedWeapon->bHasRangedMode;
	return bRangedActive ? RangedMeshComponent : MeleeMeshComponent;
}

void UGSWeaponComponent::SetTorchReadied(bool bNewReadied)
{
	// The Torch SLOT outranks a caller asking to un-ready.
	//
	// UGSGA_TorchToss readies on activation and un-readies the instant the projectile spawns, which
	// is Michael's "it's never in your hand" - the prop was visible for TorchWindupSeconds, 0.25s,
	// and the socket and mesh were fine all along. Now that the torch is a held slot rather than a
	// loan during an ability, the ability throwing one must not take the slot's torch away. It comes
	// back for the next throw because the goblin is still holding it.
	//
	// Readying while in another slot is still allowed, so the old ability-driven behaviour survives
	// untouched for anything that has not moved to slots.
	if (!bNewReadied && CurrentSlot == EGSWeaponSlot::Torch)
	{
		return;
	}

	if (bTorchReadied == bNewReadied)
	{
		return;
	}
	bTorchReadied = bNewReadied;

	// Lazy creation on the first ready. A goblin who never throws a torch never builds the
	// component, and one who does builds it once and keeps it for the rest of the raid - the torch
	// is thrown often enough that destroying it on every throw would be pure churn.
	if (bTorchReadied && EquippedWeapon)
	{
		EnsureWeaponMeshComponent(HeldTorchMeshComponent, EquippedWeapon->HeldTorchMesh,
			bHeldTorchMeshResolveFailed, TEXT("held torch"));
	}

	if (HeldTorchMeshComponent)
	{
		// Hidden rather than destroyed when un-readied: this is the same prop coming back in a
		// second or two, and visibility is free where a destroy/recreate is not.
		HeldTorchMeshComponent->SetVisibility(bTorchReadied, true);
		if (bTorchReadied && EquippedWeapon)
		{
			AttachWeaponMeshToSocket(HeldTorchMeshComponent, EquippedWeapon->HeldTorchSocket,
				EquippedWeapon->HeldTorchMeshOffset);
		}
	}
}

void UGSWeaponComponent::SetHornRaised(bool bNewRaised)
{
	// No slot guard, unlike SetTorchReadied. The torch needed one because the Torch SLOT owns its
	// prop and an ability must not take it away; the horn is owned by nothing and belongs to the
	// blast that raised it, so the caller is always right.
	if (bHornRaised == bNewRaised)
	{
		return;
	}
	bHornRaised = bNewRaised;

	if (bHornRaised && EquippedWeapon)
	{
		EnsureWeaponMeshComponent(HornMeshComponent, EquippedWeapon->HornMesh,
			bHornMeshResolveFailed, TEXT("war horn"));
	}

	if (HornMeshComponent)
	{
		// Hidden rather than destroyed, same reasoning as the torch: this prop is coming back.
		HornMeshComponent->SetVisibility(bHornRaised, true);
		if (bHornRaised && EquippedWeapon)
		{
			AttachWeaponMeshToSocket(HornMeshComponent, EquippedWeapon->HornSocket,
				EquippedWeapon->HornMeshOffset);
		}
	}
}

void UGSWeaponComponent::RebuildWeaponMeshes()
{
	if (!EquippedWeapon)
	{
		DestroyWeaponMeshes();
		return;
	}

	// Each of these is independently allowed to end up null - unset field, missing asset, or a
	// weapon that simply has no ranged half. Nothing below depends on any of them existing.
	EnsureWeaponMeshComponent(MeleeMeshComponent, EquippedWeapon->MeleeMesh,
		bMeleeMeshResolveFailed, TEXT("melee weapon"));
	EnsureWeaponMeshComponent(RangedMeshComponent, EquippedWeapon->RangedMesh,
		bRangedMeshResolveFailed, TEXT("ranged weapon"));
	EnsureWeaponMeshComponent(QuiverMeshComponent, EquippedWeapon->QuiverMesh,
		bQuiverMeshResolveFailed, TEXT("quiver"));

	// The held torch is NOT built here - see SetTorchReadied. But if one is already up when the
	// weapon changes (a swap mid-throw), it has to be re-pointed at the new asset's mesh rather
	// than left holding the old kit's prop.
	if (bTorchReadied)
	{
		EnsureWeaponMeshComponent(HeldTorchMeshComponent, EquippedWeapon->HeldTorchMesh,
			bHeldTorchMeshResolveFailed, TEXT("held torch"));
	}

	// Same for a horn that is up when the weapon changes. Rarer than the torch case - you cannot
	// open the weapon wheel mid-blast today - but "rarer" is not "impossible", and the failure mode
	// is a goblin holding the previous kit's horn for the rest of the raid.
	if (bHornRaised)
	{
		EnsureWeaponMeshComponent(HornMeshComponent, EquippedWeapon->HornMesh,
			bHornMeshResolveFailed, TEXT("war horn"));
	}

	RefreshWeaponMeshPlacement();
}

void UGSWeaponComponent::RefreshWeaponMeshPlacement()
{
	if (!EquippedWeapon)
	{
		return;
	}

	// A weapon with no ranged half can never be "in ranged mode" visually, whatever the slot says -
	// belt and braces against a future caller that sets the slot without going through SetSlot's
	// bHasRangedMode guard.
	const bool bRangedActive = IsInRangedMode() && EquippedWeapon->bHasRangedMode;

	if (MeleeMeshComponent)
	{
		const bool bActive = !bRangedActive;
		AttachWeaponMeshToSocket(MeleeMeshComponent,
			bActive ? EquippedWeapon->MeleeSocket : EquippedWeapon->MeleeHolsterSocket,
			EquippedWeapon->MeleeMeshOffset);
		// Attached-then-hidden rather than detached: the holstered half keeps riding the skeleton
		// so that turning bShowHolsteredWeapon back on in the details panel mid-PIE shows it in the
		// right place immediately, with no re-attach.
		// THE HOLSTERED MELEE WEAPON IS HIDDEN FOR THE WHOLE TIME THE BOW IS OUT, not merely while
		// an aim is up. Hiding it on aim alone was the first attempt and Michael rejected it for a
		// reason worth keeping written down: the axe reappeared the instant the shot was loosed,
		// "so you can't see what you hit" - the pop lands exactly on the moment the player is trying
		// to read. `back_sword` is on Spine02 at (12, 6, 30) with the axe at 1.6 scale, which is
		// where the over-the-shoulder camera looks.
		//
		// `!bAimActive` is kept as well, so aiming the TORCH also clears the axe off the back - the
		// same camera, the same problem, and the torch is not covered by bRangedActive.
		//
		// Michael asked for this "for now": the honest fix is a holster position that clears the
		// camera, at which point both of these conditions can go.
		//
		// bActive is still ORed in front, so a weapon actually IN HAND is never hidden by either.
		const bool bShowHolstered =
			EquippedWeapon->bShowHolsteredWeapon && !bAimActive && !bRangedActive;
		MeleeMeshComponent->SetVisibility(bActive || bShowHolstered, true);
	}

	if (RangedMeshComponent)
	{
		const bool bActive = bRangedActive;
		AttachWeaponMeshToSocket(RangedMeshComponent,
			bActive ? EquippedWeapon->RangedSocket : EquippedWeapon->RangedHolsterSocket,
			EquippedWeapon->RangedMeshOffset);
		RangedMeshComponent->SetVisibility(bActive || EquippedWeapon->bShowHolsteredWeapon, true);
	}

	if (QuiverMeshComponent)
	{
		// THE QUIVER NEVER MOVES and is never hidden by the swap. bShowHolsteredWeapon governs the
		// holstered half of the sword⇄bow pair only - the quiver is not holstered, it is worn, and
		// a Scout with the bow in hand still has arrows on his back.
		AttachWeaponMeshToSocket(QuiverMeshComponent, EquippedWeapon->QuiverSocket,
			EquippedWeapon->QuiverMeshOffset);
		QuiverMeshComponent->SetVisibility(true, true);
	}

	if (HeldTorchMeshComponent)
	{
		AttachWeaponMeshToSocket(HeldTorchMeshComponent, EquippedWeapon->HeldTorchSocket,
			EquippedWeapon->HeldTorchMeshOffset);
		HeldTorchMeshComponent->SetVisibility(bTorchReadied, true);
	}
}

UStaticMeshComponent* UGSWeaponComponent::EnsureWeaponMeshComponent(TObjectPtr<UStaticMeshComponent>& Slot,
	const TSoftObjectPtr<UStaticMesh>& SoftMesh, bool& bResolveFailedLatch, const TCHAR* SlotLabel)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// An unset field is a legal, SILENT state - it is how three quarters of the kit ships today and
	// how a great-club (no bow, no quiver) will ship forever. Only a set-but-unresolvable path is
	// worth a warning.
	if (SoftMesh.IsNull())
	{
		if (Slot)
		{
			Slot->DestroyComponent();
			Slot = nullptr;
		}
		return nullptr;
	}

	if (bResolveFailedLatch)
	{
		return nullptr;
	}

	UStaticMesh* Mesh = SoftMesh.LoadSynchronous();
	if (!Mesh)
	{
		bResolveFailedLatch = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s: the %s mesh (%s) could not be loaded - this goblin will fight "
				 "with that slot empty. Everything else about the weapon (damage, turn rate, "
				 "abilities) is unaffected."),
			*Owner->GetName(), SlotLabel, *SoftMesh.ToString());
		if (Slot)
		{
			Slot->DestroyComponent();
			Slot = nullptr;
		}
		return nullptr;
	}

	if (!Slot)
	{
		Slot = NewObject<UStaticMeshComponent>(Owner);
		if (!Slot)
		{
			return nullptr;
		}
		// Props, not colliders. Weapon hits are traced by the attack abilities (which is what
		// GetActiveWeaponMesh exists for); a collider on a sword swinging through a doorway would
		// fight the character movement component for no gain.
		Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slot->SetGenerateOverlapEvents(false);
		Slot->SetIsReplicated(false); // cosmetic, rebuilt locally on every machine from the data asset
		Slot->RegisterComponent();    // must precede AttachToComponent for a runtime-created component
	}

	Slot->SetStaticMesh(Mesh);
	return Slot;
}

void UGSWeaponComponent::AttachWeaponMeshToSocket(UStaticMeshComponent* MeshComp, FName SocketName,
	const FTransform& Offset)
{
	if (!MeshComp)
	{
		return;
	}

	// ACharacter rather than AGSCharacterBase: GetMesh() is ACharacter's, and there is no reason a
	// weapon component dropped on a plain character shouldn't draw its weapon.
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* OwnerMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!OwnerMesh)
	{
		return;
	}

	// DoesSocketExist covers both real sockets and bare bone names, which is exactly the latitude
	// wanted here: "hand_r_weapon" may end up being an authored socket or simply the name of a bone
	// somebody added, and either is a legitimate answer to "where does the sword go".
	const bool bSocketExists = !SocketName.IsNone() && OwnerMesh->DoesSocketExist(SocketName);

	if (!bSocketExists && !WarnedMissingSockets.Contains(SocketName))
	{
		WarnedMissingSockets.Add(SocketName);
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s: socket '%s' does not exist on skeletal mesh '%s' - attaching "
				 "'%s' to the character root instead, so it will sit at the goblin's feet rather "
				 "than in his hand. This is the expected state until the weapon sockets are "
				 "authored on the goblin skeleton; the fallback is deliberately VISIBLE so it "
				 "cannot be mistaken for a mesh that failed to load."),
			*OwnerCharacter->GetName(), *SocketName.ToString(),
			*GetNameSafe(OwnerMesh->GetSkinnedAsset()), *MeshComp->GetName());
	}

	// Fall back to the character's root, NOT to the skeletal mesh with NAME_None: the root is a
	// place nobody could mistake for correct, whereas an unsocketed attach to the mesh puts the
	// weapon at the pawn's origin looking almost plausible.
	USceneComponent* AttachParent = bSocketExists ? Cast<USceneComponent>(OwnerMesh)
												  : OwnerCharacter->GetRootComponent();
	if (!AttachParent)
	{
		return;
	}

	MeshComp->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetIncludingScale,
		bSocketExists ? SocketName : NAME_None);

	// The snap zeroes the relative transform; the per-weapon offset then rides on top of it. An
	// unset (identity) offset therefore means "exactly what the socket says", which is what a
	// correctly authored socket should need - see UGSWeaponDataAsset::MeleeMeshOffset.
	MeshComp->SetRelativeTransform(Offset);
}

void UGSWeaponComponent::DestroyWeaponMeshes()
{
	// Explicit rather than a loop over an array: four named slots is fewer moving parts than a
	// container, and each one is nulled as it goes so a double call is harmless.
	if (MeleeMeshComponent)     { MeleeMeshComponent->DestroyComponent();     MeleeMeshComponent = nullptr; }
	if (RangedMeshComponent)    { RangedMeshComponent->DestroyComponent();    RangedMeshComponent = nullptr; }
	if (QuiverMeshComponent)    { QuiverMeshComponent->DestroyComponent();    QuiverMeshComponent = nullptr; }
	if (HeldTorchMeshComponent) { HeldTorchMeshComponent->DestroyComponent(); HeldTorchMeshComponent = nullptr; }
	if (HornMeshComponent)      { HornMeshComponent->DestroyComponent();      HornMeshComponent = nullptr; }

	bTorchReadied = false;
	bHornRaised = false;
}

void UGSWeaponComponent::AddBloodOrb()
{
	BloodOrbs = FMath::Min(BloodOrbs + 1, MaxBloodOrbs);
	OnOrbCountChanged.Broadcast(BloodOrbs);
}

bool UGSWeaponComponent::ConsumeBloodOrbs(int32 Count)
{
	if (Count <= 0 || BloodOrbs < Count)
	{
		return false;
	}

	BloodOrbs -= Count;
	OnOrbCountChanged.Broadcast(BloodOrbs);
	return true;
}
