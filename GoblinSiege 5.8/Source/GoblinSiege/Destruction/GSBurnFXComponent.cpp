#include "Destruction/GSBurnFXComponent.h"
#include "Destruction/GSBurnMaskSubsystem.h"
#include "Destruction/GSFlammableComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGSBurnFXComponent::UGSBurnFXComponent()
{
	// Timer-driven like every other burn system in this module. Char updates ten times a second
	// are indistinguishable from per-frame ones and cost nothing on a village full of props.
	PrimaryComponentTick.bCanEverTick = false;

	// Soft path, same degrade-not-crash rule as every other FX reference in this project - see
	// AGSFireVolume::FireSystem. P_SmolderSmoke_Converted (2026-08-29) - Michael's own Niagara
	// conversion (Cascade To Niagara Converter plugin) of DreamscapeFarmlands' P_SmolderSmoke, so
	// the look stays purpose-built for smouldering debris while keeping this project Niagara-only.
	SmolderSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/DreamscapeSeries/DreamscapeFarmlands/Particles/P_SmolderSmoke_Converted.P_SmolderSmoke_Converted")));
}

void UGSBurnFXComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// MIDs first: they must exist before any delegate can fire, and a prop that never burns still
	// wants them so a debug scrub or a designer preview has something to talk to.
	CacheMaterialInstances();

	FlammableComp = Owner->FindComponentByClass<UGSFlammableComponent>();
	if (!FlammableComp)
	{
		// Not an error - this component is meant to be droppable on anything, and plenty of props
		// will wear it before someone gets round to making them flammable.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s has a UGSBurnFXComponent but no UGSFlammableComponent - it will never char."),
			*Owner->GetName());
		return;
	}

	FlammableComp->OnIgnited.AddDynamic(this, &UGSBurnFXComponent::HandleIgnited);
	FlammableComp->OnExtinguished.AddDynamic(this, &UGSBurnFXComponent::HandleExtinguished);
	FlammableComp->OnBurnedDown.AddDynamic(this, &UGSBurnFXComponent::HandleBurnedDown);

	// Late-spawned FX (streamed-in level, respawned prop) can attach to something already alight or
	// already ash. Catch up rather than starting clean, or a reload would scrub the village pristine.
	if (FlammableComp->HasBurnedDown())
	{
		HandleBurnedDown();
	}
	else if (FlammableComp->IsBurning())
	{
		HandleIgnited();
	}
}

void UGSBurnFXComponent::CacheMaterialInstances()
{
	// CharTargetActor, when set, redirects char to a different actor's meshes entirely - see
	// SetCharTargetActor. Falls back to the owner, which is every user of this component except
	// the mill.
	AActor* Target = CharTargetActor.IsValid() ? CharTargetActor.Get() : GetOwner();
	if (!Target)
	{
		return;
	}

	// UMeshComponent is the common base of StaticMesh, SkeletalMesh AND InstancedStaticMesh, so one
	// pass covers hand-placed props, characters and the generator's instanced scatter alike.
	TArray<UMeshComponent*> MeshComps;
	Target->GetComponents<UMeshComponent>(MeshComps);

	BurnMIDs.Reset();

	for (UMeshComponent* Mesh : MeshComps)
	{
		if (!Mesh)
		{
			continue;
		}

		const int32 NumSlots = Mesh->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			// An empty slot yields a null MID and a log spam warning from the engine; skip it.
			if (!Mesh->GetMaterial(SlotIndex))
			{
				continue;
			}

			if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex))
			{
				BurnMIDs.Add(MID);
			}
		}
	}
}

void UGSBurnFXComponent::HandleIgnited()
{
	// Ash does not re-char. Once burned down the look is final, whatever the flammable does next.
	if (bCharLocked)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || FXTickInterval <= 0.f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(BurnFXTimerHandle, this,
		&UGSBurnFXComponent::BurnFXTick, FXTickInterval, true);

	// Smoke starts the moment this catches, not only once it has fully burned down - a building
	// that is actively on fire needs to read as on fire from outside, same reasoning as the
	// position fix in SpawnSmolder(). SpawnSmolder() is itself idempotent (early-outs if SmolderFX
	// already exists), so this and the burn-down call in HandleBurnedDown() cannot double-spawn.
	SpawnSmolder();

	// Push once immediately so a re-ignited half-burned prop doesn't visibly pop on the first tick.
	BurnFXTick();
}

void UGSBurnFXComponent::HandleExtinguished()
{
	// Freeze, do NOT rewind. A doused building stays scorched at whatever it reached - the
	// defenders saved it, they didn't repaint it. Mirrors UGSFlammableComponent::Extinguish(),
	// which deliberately keeps its burn progress for the same reason.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnFXTimerHandle);
	}
}

void UGSBurnFXComponent::HandleBurnedDown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnFXTimerHandle);
	}

	// Clamp to the authored final char and lock it there for the rest of the raid.
	bCharLocked = true;
	CurrentBurnAmount = FMath::Clamp(FinalBurnAmount, 0.f, 1.f);
	ApplyToMaterials(CurrentBurnAmount);

	SpawnSmolder();

	// The mark on the GROUND, new on 2026-07-31. Char is on this actor's own materials; the world
	// remembers separately, and now so does the dirt under it. Runs here rather than in
	// BurnFXTick because a burnt-down prop is a one-shot event and the mask's R channel is an
	// accumulator - see SplatGroundScorch.
	SplatGroundScorch();
}

void UGSBurnFXComponent::SplatGroundScorch()
{
	AActor* Owner = GetOwner();
	if (!bSplatGroundScorch || !Owner)
	{
		return;
	}

	// Null on a dedicated server, where the subsystem is deliberately never created, and on any
	// non-game world. Never an error - the same null-safety contract every other caller honours.
	UGSBurnMaskSubsystem* Mask = UGSBurnMaskSubsystem::Get(this);
	if (!Mask)
	{
		return;
	}

	// Bounds rather than a fixed radius, for the same reason SpawnSmolder scales the plume: a
	// haycart and a windmill both wear this component, and one scorch size cannot be right for
	// both. GetActorBounds with bOnlyCollidingComponents=false so a purely visual mesh still
	// contributes - the mark should match what the player SEES burn, not what blocks them.
	FVector Origin = FVector::ZeroVector;
	FVector BoxExtent = FVector::ZeroVector;
	Owner->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, BoxExtent);

	// static_cast because FVector is double-precision under LWC and FMath::Max returns a double
	// here; initialising a float from it is a narrowing conversion warning.
	const float Footprint = static_cast<float>(FMath::Max(BoxExtent.X, BoxExtent.Y));
	const float Radius = FMath::Max(MinScorchRadius, Footprint * FMath::Max(0.1f, ScorchRadiusScale));

	// Burnt01 = 1: this is a terminal, one-shot event, so the honest deposit is "fully charred",
	// unlike a field cell which deposits its char in slices as it burns. Ember 0 and doused 0 -
	// the smolder plume from SpawnSmolder carries the lingering fire read, and an ember on the
	// mask would be re-splatted by nobody and fade out within the second anyway.
	//
	// Splatted at the BOUNDS ORIGIN rather than the actor location: an actor pivot is frequently at
	// one corner of a building, and a scorch offset by half a granary is worse than none.
	Mask->SplatBurn(Origin, Radius, /*Burnt01=*/1.f, /*Ember01=*/0.f, /*Doused01=*/0.f);
}

void UGSBurnFXComponent::BurnFXTick()
{
	if (!FlammableComp)
	{
		return;
	}

	const float Value = FMath::Clamp(EvaluateResponse(FlammableComp->GetBurnProgress01()), 0.f, 1.f);

	// Ratchet: char only ever deepens. Protects the look from any future source of non-monotonic
	// burn progress (a re-ignite that resets, a replicated value arriving out of order).
	if (Value <= CurrentBurnAmount)
	{
		return;
	}

	CurrentBurnAmount = Value;
	ApplyToMaterials(CurrentBurnAmount);
}

float UGSBurnFXComponent::EvaluateResponse(float RawProgress01) const
{
	if (BurnResponseCurve)
	{
		return BurnResponseCurve->GetFloatValue(RawProgress01);
	}
	return RawProgress01;
}

void UGSBurnFXComponent::ApplyToMaterials(float Value)
{
	// Setting a scalar parameter a material does not declare is a safe no-op in UE - it resolves
	// against nothing and costs a hash lookup. THIS is why the component can be attached to the
	// entire prop kit before any of the materials have opted in: the ones without GS_BurnAmount
	// simply ignore us, and they start charring the day the art side wires the node up.
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : BurnMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(BurnAmountParam, Value);
		}
	}
}

void UGSBurnFXComponent::SetBurnAmount(float NewValue01)
{
	CurrentBurnAmount = FMath::Clamp(NewValue01, 0.f, 1.f);
	ApplyToMaterials(CurrentBurnAmount);
}

void UGSBurnFXComponent::SetCharTargetActor(AActor* NewTarget)
{
	CharTargetActor = NewTarget;

	// Re-cache against the new target immediately, then re-push whatever char level this component
	// already reached - a caller that resolves its geometry mid-fuse (the mill can't know it at
	// construction) should not have to wait for the next BurnFXTick to see the redirect take effect.
	CacheMaterialInstances();
	ApplyToMaterials(CurrentBurnAmount);
}

void UGSBurnFXComponent::SpawnSmolder()
{
	AActor* Owner = GetOwner();
	if (!Owner || SmolderFX)
	{
		return;
	}

	USceneComponent* AttachRoot = Owner->GetRootComponent();
	if (!AttachRoot)
	{
		return;
	}

	UNiagaraSystem* System = SmolderSystem.LoadSynchronous();
	if (!System)
	{
		// Degrade to "burnt but no smoke" - the char alone still reads. Warning only, never fatal.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s burned down but has no smolder Niagara system (%s) - no lingering smoke."),
			*Owner->GetName(), *SmolderSystem.ToString());
		return;
	}

	// Spawned at the CENTRE of the actor's bounds, in WORLD space - not at the root component's own
	// origin (a house's pivot sits at ground level, or wherever the kit piece was authored, which
	// reads as "no smoke visible from outside" the same way a top-only spawn once did) and not at
	// the top either. Michael, 2026-08-29, watching the plume once it was actually visible: it is
	// large enough on its own to billow up past the roofline from a source INSIDE the structure -
	// sourcing it from the top instead made a big cloud look like it was floating above the house
	// rather than pouring out of it. GetActorBounds' Origin is already the bounding box's centre.
	FVector Origin = FVector::ZeroVector;
	FVector BoxExtent = FVector::ZeroVector;
	Owner->GetActorBounds(/*bOnlyCollidingComponents=*/ false, Origin, BoxExtent);
	const FVector SpawnLocation = Origin;

	SmolderFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Owner->GetWorld(),
		System,
		SpawnLocation,
		FRotator::ZeroRotator,
		FVector(1.0),
		/*bAutoDestroy=*/ false,
		/*bAutoActivate=*/ true);

	if (!SmolderFX)
	{
		return;
	}

	// Attach AFTER spawn so it still tracks the actor (a pooled/repositioned owner, or rubble that
	// settles after crumbling) without inheriting the root's rotation - smoke should rise straight
	// up regardless of which way a collapsed house happens to be lying.
	SmolderFX->AttachToComponent(AttachRoot, FAttachmentTransformRules::KeepWorldTransform);

	// A haycart and a windmill both wear this component, so the plume has to be proportionate or
	// one of them gets a bonfire's worth of smoke. Same correction as
	// AGSFireVolume::FireSystemAuthoredRadius: measured footprint over the authored footprint.
	float FinalScale = SmolderScale;
	if (SmolderAuthoredRadius > 0.f)
	{
		// static_cast because FVector is double-precision under LWC, so FMath::Max returns a
		// double here and initialising a float from it is a narrowing conversion warning.
		const float FootprintRadius = static_cast<float>(FMath::Max(BoxExtent.X, BoxExtent.Y));
		if (FootprintRadius > KINDA_SMALL_NUMBER)
		{
			FinalScale *= FMath::Max(0.1f, FootprintRadius / SmolderAuthoredRadius);
		}
	}

	SmolderFX->SetWorldScale3D(FVector(FinalScale));

	// Forever by default: a razed place must still read as razed at raid end, not quietly tidy
	// itself up while the player is off burning the next field.
	if (!bSmolderForever && SmolderDurationSeconds > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SmolderStopHandle, this,
				&UGSBurnFXComponent::StopSmolder, SmolderDurationSeconds, false);
		}
	}
}

void UGSBurnFXComponent::StopSmolder()
{
	if (SmolderFX)
	{
		// Deactivate rather than destroy so particles already in the air finish their life instead
		// of vanishing mid-column.
		SmolderFX->Deactivate();
	}
}

void UGSBurnFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnFXTimerHandle);
		World->GetTimerManager().ClearTimer(SmolderStopHandle);
	}

	if (FlammableComp)
	{
		FlammableComp->OnIgnited.RemoveDynamic(this, &UGSBurnFXComponent::HandleIgnited);
		FlammableComp->OnExtinguished.RemoveDynamic(this, &UGSBurnFXComponent::HandleExtinguished);
		FlammableComp->OnBurnedDown.RemoveDynamic(this, &UGSBurnFXComponent::HandleBurnedDown);
		FlammableComp = nullptr;
	}

	// SmolderFX is attached to the owner and dies with it; nothing to tear down here.
	BurnMIDs.Reset();

	Super::EndPlay(EndPlayReason);
}
