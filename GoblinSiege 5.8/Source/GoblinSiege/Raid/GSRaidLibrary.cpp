#include "Raid/GSRaidLibrary.h"
#include "Raid/GSRaidMarker.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Destruction/GSFlammableComponent.h"
#include "Destruction/GSBreakableComponent.h"
#include "Destruction/GSBurnFXComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSRaidScripting, Log, All);

FGameplayTag UGSRaidLibrary::MakeTagByName(FName TagName, bool bErrorIfNotFound)
{
	if (TagName.IsNone())
	{
		return FGameplayTag();
	}

	// RequestGameplayTag with ErrorIfNotFound=false, then decide ourselves whether to complain -
	// the engine's own assert on a missing tag would take the editor down, and a script mistyping
	// a tag deserves a log line, not a crash.
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);

	if (!Tag.IsValid() && bErrorIfNotFound)
	{
		UE_LOG(LogGSRaidScripting, Error,
			TEXT("[GoblinSiege] Gameplay tag '%s' is not registered. Check spelling against ")
			TEXT("Combat/GSGameplayTags.cpp - an empty tag leaves the carrier invisible to the ")
			TEXT("win condition, which looks like a working level that cannot be won."),
			*TagName.ToString());
	}

	return Tag;
}

bool UGSRaidLibrary::SetObjectiveIdentity(AGSBurnObjectiveBase* Objective, FName TypeTagName, FText DisplayName)
{
	if (!Objective)
	{
		return false;
	}

	const FGameplayTag Tag = MakeTagByName(TypeTagName, true);
	if (!Tag.IsValid())
	{
		return false;
	}

	Objective->SetObjectiveIdentity(Tag, DisplayName);

	UE_LOG(LogGSRaidScripting, Log, TEXT("[GoblinSiege] MODIFIED %s -> %s '%s'"),
		*Objective->GetName(), *TypeTagName.ToString(), *DisplayName.ToString());
	return true;
}

bool UGSRaidLibrary::ConfigureMarker(AGSRaidMarker* Marker, FName MarkerTypeName, FName GroupId,
	int32 OrderIndex, float Radius)
{
	if (!Marker)
	{
		return false;
	}

	const FGameplayTag Tag = MakeTagByName(MarkerTypeName, true);
	if (!Tag.IsValid())
	{
		return false;
	}

	Marker->Configure(Tag, GroupId, OrderIndex, Radius);
	return true;
}

namespace
{
	/**
	 * Give an actor its char, if it has none.
	 *
	 * Separated out and called on BOTH paths below, because the first version of this put it after
	 * MakeActorFlammable's idempotency guard - so an actor that ALREADY had a flammable component
	 * returned early and never got char. Re-running the dressing script on the 67 market stalls
	 * therefore fixed nothing, silently, which is the exact failure mode this whole area keeps
	 * producing.
	 */
	void EnsureBurnFX(AActor* Actor)
	{
		if (!Actor || Actor->FindComponentByClass<UGSBurnFXComponent>())
		{
			return;
		}

		UGSBurnFXComponent* FX = NewObject<UGSBurnFXComponent>(Actor, UGSBurnFXComponent::StaticClass(),
			TEXT("GSBurnFX"), RF_Transactional);
		if (FX)
		{
			Actor->AddInstanceComponent(FX);
			FX->RegisterComponent();
			Actor->MarkPackageDirty();
		}
	}
}

UGSFlammableComponent* UGSRaidLibrary::MakeActorFlammable(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Char first, and unconditionally - see EnsureBurnFX.
	EnsureBurnFX(Actor);

	// Idempotent. A dressing script gets re-run - after a crash, after a tweak, after a second pass
	// over a wider radius - and stacking a second flammable component on a stall would double every
	// spread roll it makes.
	if (UGSFlammableComponent* Existing = Actor->FindComponentByClass<UGSFlammableComponent>())
	{
		return Existing;
	}

	UGSFlammableComponent* Comp = NewObject<UGSFlammableComponent>(Actor, UGSFlammableComponent::StaticClass(),
		TEXT("GSFlammable"), RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}

	// AddInstanceComponent is the part that matters: without it the component is a transient runtime
	// object that vanishes on reload, so the level LOOKS dressed until you close the editor.
	Actor->AddInstanceComponent(Comp);
	Comp->RegisterComponent();

	Actor->MarkPackageDirty();

	return Comp;
}

UGSBreakableComponent* UGSRaidLibrary::MakeActorBreakable(AActor* Actor, bool bOpensBuilding)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UGSBreakableComponent* Existing = Actor->FindComponentByClass<UGSBreakableComponent>())
	{
		return Existing;
	}

	UGSBreakableComponent* Comp = NewObject<UGSBreakableComponent>(Actor, UGSBreakableComponent::StaticClass(),
		TEXT("GSBreakable"), RF_Transactional);
	if (!Comp)
	{
		return nullptr;
	}

	// AddInstanceComponent is the whole point - see the header.
	Actor->AddInstanceComponent(Comp);
	Comp->RegisterComponent();
	Comp->SetOpensBuilding(bOpensBuilding);

	Actor->MarkPackageDirty();
	return Comp;
}

int32 UGSRaidLibrary::CountBreakable(const TArray<AActor*>& Actors)
{
	int32 Count = 0;
	for (const AActor* Actor : Actors)
	{
		if (Actor && Actor->FindComponentByClass<UGSBreakableComponent>())
		{
			++Count;
		}
	}
	return Count;
}

int32 UGSRaidLibrary::CountFlammable(const TArray<AActor*>& Actors)
{
	int32 Count = 0;
	for (const AActor* Actor : Actors)
	{
		if (Actor && Actor->FindComponentByClass<UGSFlammableComponent>())
		{
			++Count;
		}
	}
	return Count;
}
