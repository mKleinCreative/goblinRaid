/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadFbxBuildHost.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "MetaRoadActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadFbxBuildHost)

AActor* UMetaRoadFbxBuildHost::BuildTransientExportActor(UWorld* InWorld, AMetaRoad* Road)
{
	if (!InWorld || !Road)
	{
		return nullptr;
	}
	World = InWorld;

	// Ensure the actor's real build settings exist (game thread) before the synchronous build reads them.
	if (UMetaRoadBuildSettings* Settings = UMetaRoadBuildSettings::GetForActor(Road, /*bCreateIfMissing=*/true))
	{
		Settings->EnsureDefaultPropertySets();
	}

	// Build the whole pipeline stack synchronously in RAM (no background threading, no asset generation yet).
	SetSplineActors({ TWeakObjectPtr<AMetaRoad>(Road) });
	InitializePipelines();
	BuildAllSynchronous();

	const FTransform RoadXf = Road->GetActorTransform();

	// Spawn a throwaway transient actor to receive the generated mesh components. Hidden from the outliner and
	// never serialized; the exporter destroys it once written.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bTemporaryEditorActor = true;
	SpawnParams.bHideFromSceneOutliner = true;
	AActor* OutputActor = InWorld->SpawnActor<AActor>(AActor::StaticClass(), RoadXf, SpawnParams);
	if (!OutputActor)
	{
		ResetPipelines();
		return nullptr;
	}
	if (!OutputActor->GetRootComponent())
	{
		USceneComponent* Root = NewObject<USceneComponent>(OutputActor, TEXT("Root"));
		OutputActor->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	OutputActor->SetActorTransform(RoadXf);
	// Freeze the root Static after positioning: the generated mesh components are Static, so the root must be
	// Static too or AttachTo aborts (mirrors the bake host setting the _Gen actor root to Static); positioning
	// while Movable avoids a "static component moved" warning in a PIE world.
	if (USceneComponent* Root = OutputActor->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Static);
	}

	// Materialize each pipeline's layers onto the actor. WantsTransientMeshOutput() == true routes UStaticMesh
	// creation to the transient package. ActorToWorld = the road's world transform (the ops bake into that local
	// space, and OutputActor is at that transform → components land at their original world positions).
	const FTransform3d ActorToWorld = (FTransform3d)RoadXf;
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		if (Pipeline.IsValid())
		{
			Pipeline->GenerateAssets(OutputActor, ActorToWorld);
		}
	}

	ResetPipelines();
	return OutputActor;
}
