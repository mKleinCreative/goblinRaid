/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadSubsystem.h"

#if WITH_EDITOR

#include "RoadSplineComponent.h"
#include "EngineUtils.h"
#include "MetaRoadModule.h"
#include "Editor.h"

using namespace MetaRoad;

#endif // WITH_EDITOR

TStatId UMetaRoadSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMetaRoadSubsystem, STATGROUP_Tickables);
}

#if WITH_EDITOR

void UMetaRoadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEditor->OnBeginObjectMovement().AddUObject(this, &UMetaRoadSubsystem::BeginObjectMovement);
		GEditor->OnActorsMoved().AddUObject(this, &UMetaRoadSubsystem::EndObjectMovement);

		FEditorDelegates::OnDuplicateActorsBegin.AddUObject(this, &UMetaRoadSubsystem::OnDuplicateActorsBegin);
		FEditorDelegates::OnEditPasteActorsBegin.AddUObject(this, &UMetaRoadSubsystem::OnDuplicateActorsBegin);
	}

	Super::Initialize(Collection);
}

void UMetaRoadSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEditor->OnBeginObjectMovement().RemoveAll(this);
		GEditor->OnEndObjectMovement().RemoveAll(this);

		FEditorDelegates::OnDuplicateActorsBegin.RemoveAll(this);
		FEditorDelegates::OnEditPasteActorsBegin.RemoveAll(this);
	}
}

void UMetaRoadSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PendingSplinesToImport.Num())
	{
		TArray<URoadSplineComponent*> Splines;
		for (auto& It : PendingSplinesToImport)
		{
			if (It.IsValid())
			{
				Splines.Add(It.Get());
			}
		}
		EndCopySplineTransaction(Splines);
		PendingSplinesToImport.Empty();
	}
}

void UMetaRoadSubsystem::EndCopySplineTransaction(const TArray<URoadSplineComponent*>& Splines)
{
	TMap<FGuid, ULaneConnection*> Links;

	for(auto& Comp: Splines)
	{
		for (auto& Section : Comp->GetLaneSections())
		{
			auto AddLC = [&](ULaneConnection* LC)
			{
				if (LC && LC->GetGuid().IsValid())
				{
					Links.Emplace(LC->GetGuid(), LC);
				}
			};
			for (auto& Lane : Section.Left)  
			{ 
				AddLC(Lane.PredecessorConnection); 
				AddLC(Lane.SuccessorConnection); 
			}
			for (auto& Lane : Section.Right) 
			{
				AddLC(Lane.PredecessorConnection); 
				AddLC(Lane.SuccessorConnection); 
			}
		}
	};

	for (auto* Spline : Splines)
	{
		auto TryConnect = [&](URoadConnection* RC)
		{
			if (!RC || !RC->LaneConnectionGuid.IsValid()) 
			{
				return;
			}
			if (auto* LC = Links.Find(RC->LaneConnectionGuid))
			{
				RC->ConnectTo(*LC);
			}
		};
		TryConnect(Spline->GetPredecessorConnection());
		TryConnect(Spline->GetSuccessorConnection());
	}
}


void UMetaRoadSubsystem::OnDuplicateActorsBegin()
{
	// When duplicating an Actor, the connection GUIDs must be updated before serialization/deserialization. 
	// For copy-paste operations, this is done in URoadSplineComponent::PreDuplicate(). 
	// However, PreDuplicate() is not called when duplicating actors. 
	// I couldn't find suitable hooks for this in the UObject API. Therefore, we create a hook for all actors on the scene during duplication operations.
	// It's worth noting that this isn't the most optimal solution, as it requires iterating through all actors. 
	// TODO: It might be worth storing a set of registered splines in the UMetaRoadSubsystem.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		It->ForEachComponent<URoadSplineComponent>(true, [&](URoadSplineComponent* Comp)
		{
			Comp->RefreshConnectionGuids();
			Comp->ConnectionGuidSnapshot.Reset();
		});
	}
}

void UMetaRoadSubsystem::BeginObjectMovement(UObject& Object)
{
	AActor* Actor = Cast<AActor>(&Object);
	if (!Actor)
	{
		return;
	}

	if (!Actor->GetComponentByClass<URoadSplineComponent>())
	{
		return;
	}

	MovingActors.Add(Actor);
}

void UMetaRoadSubsystem::EndObjectMovement(TArray<AActor*>& Actors)
{
	if (MovingActors.Num() > 1)
	{
		for (auto& Actor : MovingActors)
		{
			Actor->ForEachComponent<URoadSplineComponent>(true, [&](URoadSplineComponent* Comp)
			{
				Comp->UpdateMagicTransform(ERoadSplineMagicTransformFilter::OuterOnly);
			});
		}
	}

	MovingActors.Empty();
}

void UMetaRoadSubsystem::OnSplinePostEditImport(URoadSplineComponent* Spline)
{
	PendingSplinesToImport.Add(Spline);
}

#endif // WITH_EDITOR

