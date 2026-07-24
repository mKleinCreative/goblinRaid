/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadBakeHost.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/IRoadOpCompute.h" // MetaRoad::GeneratedAssetComponentTagName
#include "Utils/AssetUtils.h" // Begin/EndAssetGenerationBatch
#include "EditorMode/MetaRoadBakeSettings.h" // UMetaRoadBakeSettings, EMetaRoadCleanStrategy
#include "MetaRoadActor.h"
#include "MetaRoadModule.h" // LogMetaRoad

#include "Editor.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "RenderingThread.h" // FlushRenderingCommands
#include "Framework/Docking/TabManager.h" // FGlobalTabmanager (Open Log hyperlink)

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadBakeHost)

#define LOCTEXT_NAMESPACE "MetaRoadBakeHost"

namespace
{
	TArray<UObject*> CollectMeshAssets(AActor* FromActor)
	{
		TArray<UObject*> Assets;
		TArray<UStaticMeshComponent*> Comps;
		FromActor->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			// Only components the bake tagged as an asset owner reference a mesh it CREATED. Untagged
			// components — e.g. USplineMeshComponent from URoadLaneAttributeSplineMeshDescriptor, or component-template
			// attributes — merely REFERENCE a user asset via a lane attribute and must never be deleted.
			// See MetaRoad::GeneratedAssetComponentTagName.
			if (!Comp->ComponentTags.Contains(FName(MetaRoad::GeneratedAssetComponentTagName)))
			{
				continue;
			}
			if (UStaticMesh* Mesh = Comp->GetStaticMesh())
			{
				if (Mesh->GetPackage() != GetTransientPackage())
				{
					Assets.AddUnique(Mesh);
				}
			}
		}
		return Assets;
	}

	// A unit of replace/clear cleanup deferred to run INSIDE the undo transaction, after its mesh assets have
	// already been disposed of OUTSIDE it. Weak ptrs because DisposeAssets() may CollectGarbage.
	struct FPendingCleanup
	{
		TWeakObjectPtr<AMetaRoad> SourceActor;
		TWeakObjectPtr<AActor> GenActor; // the previous _Gen actor
		bool IsEmpty() const { return !GenActor.IsValid(); }
	};

	// Collect what one source actor's replace/clear needs: the previous _Gen actor to destroy + the mesh
	// assets it CREATED (tagged GeneratedAssetComponentTagName, gathered by CollectMeshAssets) to dispose of.
	// User meshes referenced via lane attributes are never collected.
	FPendingCleanup CollectCleanupForActor(AMetaRoad* Src, TArray<UObject*>& OutAssets)
	{
		FPendingCleanup Pending;
		Pending.SourceActor = Src;
		if (Src && IsValid(Src->LastGeneratedActor))
		{
			OutAssets.Append(CollectMeshAssets(Src->LastGeneratedActor));
			Pending.GenActor = Src->LastGeneratedActor;
		}
		return Pending;
	}

	// Move (rename) generated mesh assets into a "_Trash" subfolder next to each, instead of deleting them.
	// Unlike ForceDeleteObjects this does NOT reset the editor undo buffer. Names are made unique so repeated
	// bakes don't collide inside _Trash.
	void MoveAssetsToTrash(const TArray<UObject*>& Assets)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TArray<FAssetRenameData> RenameData;
		for (UObject* Asset : Assets)
		{
			if (!Asset)
			{
				continue;
			}
			const FString TrashBase = FPaths::GetPath(Asset->GetPackage()->GetName()) / TEXT("_Trash") / Asset->GetName();
			FString UniquePackageName, UniqueAssetName;
			AssetTools.CreateUniqueAssetName(TrashBase, TEXT(""), UniquePackageName, UniqueAssetName);
			RenameData.Emplace(Asset, FPaths::GetPath(UniquePackageName), UniqueAssetName);
		}
		if (RenameData.Num() > 0)
		{
			AssetTools.RenameAssets(RenameData);
		}
	}

	// Dispose the collected generated assets per the configured CleanStrategy. Runs OUTSIDE the undo
	// transaction: ForceDeleteObjects resets prior undo history, so doing it before BeginTransaction keeps the
	// bake/clear transaction (which destroys the actors/components) undoable.
	void DisposeAssets(const TArray<UObject*>& Assets)
	{
		if (Assets.Num() == 0)
		{
			return;
		}
		FlushRenderingCommands();
		if (UMetaRoadBakeSettings::Get()->CleanStrategy == EMetaRoadCleanStrategy::MoveToTrash)
		{
			MoveAssetsToTrash(Assets);
		}
		else
		{
			ObjectTools::ForceDeleteObjects(Assets, /*bShowConfirmation=*/false);
		}
	}

	// Destroy the collected previous _Gen actor(s) INSIDE the undo transaction (EditorDestroyActor) so Ctrl+Z
	// restores them (valid mesh refs for MoveToTrash, empty for PermanentDelete).
	void DestroyPending(const TArray<FPendingCleanup>& Pending, UWorld* EditorWorld)
	{
		for (const FPendingCleanup& P : Pending)
		{
			AMetaRoad* Src = P.SourceActor.Get();
			if (Src)
			{
				Src->Modify();
			}
			if (AActor* Gen = P.GenActor.Get())
			{
				if (EditorWorld)
				{
					EditorWorld->EditorDestroyActor(Gen, /*bShouldModifyLevel=*/true);
				}
				else
				{
					Gen->Destroy();
				}
				if (Src)
				{
					Src->LastGeneratedActor = nullptr;
				}
			}
			if (Src)
			{
				Src->MarkPackageDirty();
			}
		}
	}
}

int32 UMetaRoadBakeHost::ClearGenerated(const TArray<TWeakObjectPtr<AMetaRoad>>& Actors)
{
	// Mirrors the Bake replace-on-regenerate cleanup, but unconditional (ignores bReplaceOnRegenerate) and
	// without generating anything afterwards. Assets are disposed per CleanStrategy BEFORE a transaction; the
	// actor/component destruction runs INSIDE it so Ctrl+Z can restore the cleared output.
	TArray<FPendingCleanup> Pending;
	TArray<UObject*> AssetsToDispose;
	TSet<AMetaRoad*> AlreadyCleaned;
	UWorld* EditorWorld = nullptr;
	for (const TWeakObjectPtr<AMetaRoad>& ActorPtr : Actors)
	{
		AMetaRoad* MetaRoadActor = ActorPtr.Get();
		if (!MetaRoadActor || AlreadyCleaned.Contains(MetaRoadActor))
		{
			continue;
		}
		AlreadyCleaned.Add(MetaRoadActor);
		if (!EditorWorld)
		{
			EditorWorld = MetaRoadActor->GetWorld();
		}
		FPendingCleanup P = CollectCleanupForActor(MetaRoadActor, AssetsToDispose);
		if (!P.IsEmpty())
		{
			Pending.Add(MoveTemp(P));
		}
	}
	if (Pending.Num() == 0)
	{
		return 0;
	}

	DisposeAssets(AssetsToDispose); // outside the transaction (see DisposeAssets)

	if (GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("MetaRoadClearAction", "MetaRoad Clear Generated"));
	}
	DestroyPending(Pending, EditorWorld);
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
	return Pending.Num();
}

void UMetaRoadBakeHost::BeginBakeAsync(UWorld* InWorld, const TArray<TWeakObjectPtr<AMetaRoad>>& Actors)
{
	World = InWorld;
	BakeActorsList = Actors;
	if (!World.IsValid() || Actors.Num() == 0)
	{
		return;
	}

	// Ensure each actor's real build settings exist on the game thread before the background build reads them.
	for (const TWeakObjectPtr<AMetaRoad>& Actor : Actors)
	{
		if (UMetaRoadBuildSettings* Settings = UMetaRoadBuildSettings::GetForActor(Actor.Get(), /*bCreateIfMissing=*/true))
		{
			Settings->EnsureDefaultPropertySets();
		}
	}

	// Kick the asynchronous build (background compute, harvested in TickBake).
	SetSplineActors(Actors);
	InitializePipelines();
	RequestRebuildAll();

	bBaking = true;
	bRebuildKicked = false;
	bCompleted = false;

	FAsyncTaskNotificationConfig Config;
	Config.TitleText = LOCTEXT("BakingRoadMeshes", "Baking Road Meshes");
	Config.ProgressText = LOCTEXT("BakeStarting", "Generating road meshes…");
	Config.bCanCancel = true;
	// Auto-close ~8s after completion (don't keep open on success/failure).
	Config.ExpireDuration = 8.0f;
	Notification = MakeUnique<FAsyncTaskNotification>(Config);
}

bool UMetaRoadBakeHost::TickBake(float DeltaTime)
{
	if (!bBaking)
	{
		return false;
	}

	// World lost mid-bake (e.g. level change) → abort.
	if (!World.IsValid())
	{
		CancelAllPipelines();
		if (Notification.IsValid())
		{
			Notification->SetComplete(LOCTEXT("BakeAbortedTitle", "Bake aborted"), FText::GetEmpty(), /*bSuccess=*/false);
		}
		Finish();
		return false;
	}

	// Cancel requested in the notification → abandon the build, leaving the previous _Gen result intact.
	if (Notification.IsValid() && Notification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel)
	{
		CancelAllPipelines();
		Notification->SetComplete(LOCTEXT("BakeCancelledTitle", "Bake cancelled"), FText::GetEmpty(), /*bSuccess=*/false);
		Finish();
		return false;
	}

	FRoadMeshBuildTickResult Result;
	TickPipelines(DeltaTime, Result);
	bRebuildKicked |= Result.bAnyRebuildStarted || Result.TotalActiveTasks > 0;

	if (Notification.IsValid())
	{
		Notification->SetProgressText(FText::Format(
			LOCTEXT("BakeProgress", "Generating road meshes… ({0} task(s))"), Result.TotalActiveTasks));
	}

	// Still running: rebuild not kicked yet, or background tasks remain.
	if (!bRebuildKicked || Result.TotalActiveTasks > 0)
	{
		return true;
	}

	// Per-source-actor worst status (error > warning > ok) from the build (geometry) results. Multiple SubGroup
	// scopes (pipelines) of the same actor collapse to its worst status. Asset-write failures (below) are
	// merged in after generation, so the final notification reflects the actual write outcome — not just the
	// pre-generation build state.
	TMap<AMetaRoad*, int32> ActorStatus; // 0 = ok, 1 = warning, 2 = error
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		AMetaRoad* SourceActor = Pipeline.IsValid() ? Pipeline->GetTargetActor() : nullptr;
		if (!SourceActor)
		{
			continue;
		}
		const UE::Geometry::FGeometryResult& Info = Pipeline->GetResultInfo();
		const int32 Status = (Info.HasFailed() || Info.Errors.Num() > 0) ? 2 : (Info.Warnings.Num() > 0 ? 1 : 0);
		int32& Worst = ActorStatus.FindOrAdd(SourceActor, 0);
		Worst = FMath::Max(Worst, Status);
	}

	// --- Build finished: generate output assets/actors synchronously (one game-thread step). ---

	// Interactive mode: pick ONE output folder up front (reused for every generated mesh). Cancelling here
	// aborts the whole bake before any irreversible work — the mode is NOT exited (bCompleted stays false).
	if (!AssetUtils::BeginAssetGenerationBatch())
	{
		if (Notification.IsValid())
		{
			Notification->SetComplete(LOCTEXT("BakeCancelledTitle", "Bake cancelled"),
				LOCTEXT("BakeCancelledBody", "No assets were generated."), /*bSuccess=*/false);
		}
		Finish();
		return false;
	}

	// Phase 1a: collect + dispose the previous bake's generated ASSETS per CleanStrategy, BEFORE the undo
	// transaction (ForceDelete resets prior undo history; keeping it outside keeps the bake undoable). Only
	// bReplaceOnRegenerate actors, deduped across SubGroup scopes.
	TArray<FPendingCleanup> Pending;
	{
		TArray<UObject*> AssetsToDispose;
		TSet<AMetaRoad*> AlreadyCleaned;
		for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
		{
			AMetaRoad* Src = Pipeline.IsValid() ? Pipeline->GetTargetActor() : nullptr;
			if (!Src || AlreadyCleaned.Contains(Src) || !Src->bReplaceOnRegenerate)
			{
				continue;
			}
			AlreadyCleaned.Add(Src);
			FPendingCleanup P = CollectCleanupForActor(Src, AssetsToDispose);
			if (!P.IsEmpty())
			{
				Pending.Add(MoveTemp(P));
			}
		}
		DisposeAssets(AssetsToDispose);
	}

	if (GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("MetaRoadBakeAction", "MetaRoad Bake"));
	}
	DestroyPending(Pending, World.Get()); // Phase 1b: destroy old actors/components inside the transaction (undoable)
	TSet<AMetaRoad*> WriteFailedActors;
	GenerateOutputs(WriteFailedActors); // Phase 2
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
	AssetUtils::EndAssetGenerationBatch();
	bCompleted = true; // ran to completion (the owner exits the mode on this; Finish() leaves it set)

	// Merge actual asset-write failures into the per-actor status (an OK build whose write failed is an error).
	for (AMetaRoad* FailedActor : WriteFailedActors)
	{
		ActorStatus.FindOrAdd(FailedActor, 0) = 2;
	}

	int32 NumOk = 0, NumWarnings = 0, NumErrors = 0;
	for (const TPair<AMetaRoad*, int32>& Pair : ActorStatus)
	{
		(Pair.Value == 2 ? NumErrors : (Pair.Value == 1 ? NumWarnings : NumOk))++;
	}
	const bool bAnyError = Result.bHasFailed || NumErrors > 0;
	const bool bAnyWarning = Result.bHasWarnings || NumWarnings > 0;

	if (Notification.IsValid())
	{
		// Per-status stat lines (only non-zero ones).
		TArray<FText> StatLines;
		if (NumOk > 0)       { StatLines.Add(FText::Format(LOCTEXT("BakeStatOk", "{0} actor(s) — OK"), NumOk)); }
		if (NumWarnings > 0) { StatLines.Add(FText::Format(LOCTEXT("BakeStatWarn", "{0} actor(s) — warning"), NumWarnings)); }
		if (NumErrors > 0)   { StatLines.Add(FText::Format(LOCTEXT("BakeStatErr", "{0} actor(s) — error"), NumErrors)); }
		const FText Stats = FText::Join(FText::FromString(TEXT("\n")), StatLines);

		if (bAnyError || bAnyWarning)
		{
			// Errors/warnings were reported during the build or asset write — surface the stats + an "Open Log"
			// hyperlink and keep the notification open so the user can inspect them.
			Notification->SetTitleText(bAnyError
				? LOCTEXT("BakeFailedTitle", "Bake completed with errors")
				: LOCTEXT("BakeWarnTitle", "Bake completed with warnings"), /*bClearProgressText=*/false);
			Notification->SetProgressText(Stats);
			Notification->SetHyperlink(
				FSimpleDelegate::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("OutputLog"))); }),
				LOCTEXT("BakeOpenLog", "Open Log"));
			// SetComplete(bool) preserves the hyperlink/text just set (the text-taking overload rebuilds the
			// state and would clear them). The notification auto-closes ~8s later (ExpireDuration in config).
			Notification->SetComplete(/*bSuccess=*/ !bAnyError);
		}
		else
		{
			Notification->SetComplete(LOCTEXT("BakeDoneTitle", "Bake complete"),
				FText::Format(LOCTEXT("BakeDoneBody", "Baked {0} road actor(s)."), NumOk), /*bSuccess=*/true);
		}
	}

	Finish();
	return false;
}

void UMetaRoadBakeHost::Finish()
{
	// GenerateAssets() already shut down each compute, so just drop the pipelines (Cancel would touch an
	// already-shutdown preview); on the Cancel/abort path CancelAllPipelines() has already run.
	ResetPipelines();
	Notification.Reset();
	BakeActorsList.Reset();
	World = nullptr;
	bBaking = false;
	bRebuildKicked = false;
}

int32 UMetaRoadBakeHost::GenerateOutputs(TSet<AMetaRoad*>& OutWriteFailedActors)
{
	if (!World.IsValid())
	{
		return 0;
	}

	// One generated _Gen actor per source actor (shared by all its SubGroup scopes).
	TMap<AMetaRoad*, AActor*> SourceToOutput;
	for (const TSharedPtr<MetaRoad::FRoadComputePipeline>& Pipeline : GetPipelines())
	{
		AMetaRoad* SourceActor = Pipeline.IsValid() ? Pipeline->GetTargetActor() : nullptr;
		if (!SourceActor)
		{
			UE_LOG(LogMetaRoad, Error, TEXT("UMetaRoadBakeHost::GenerateOutputs(); TargetActor is lost"));
			continue;
		}

		const FTransform3d ActorToWorld = (FTransform3d)SourceActor->GetTransform();

		AActor*& OutputActor = SourceToOutput.FindOrAdd(SourceActor, nullptr);
		if (!OutputActor)
		{
			const FString ActorName = SourceActor->GetActorLabel() + TEXT("_Gen");
			UActorFactoryEmptyActor* EmptyActorFactory = NewObject<UActorFactoryEmptyActor>();
			FAssetData AssetData(EmptyActorFactory->GetDefaultActorClass(FAssetData()));
			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = *ActorName;
			SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			OutputActor = EmptyActorFactory->CreateActor(AssetData.GetAsset(), World->GetCurrentLevel(), (FTransform)ActorToWorld, SpawnParams);
			if (OutputActor)
			{
				FActorLabelUtilities::SetActorLabelUnique(OutputActor, ActorName);
				OutputActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
			}
		}

		if (!OutputActor)
		{
			UE_LOG(LogMetaRoad, Error, TEXT("UMetaRoadBakeHost::GenerateOutputs(); Can't create OutputActor"));
			continue;
		}

		if (!Pipeline->GenerateAssets(OutputActor, ActorToWorld))
		{
			// A layer's asset write failed — flag the source actor so the bake notification reports an error.
			OutWriteFailedActors.Add(SourceActor);
		}
	}

	// Point each source actor at its freshly generated _Gen actor.
	for (const TPair<AMetaRoad*, AActor*>& Pair : SourceToOutput)
	{
		AMetaRoad* MetaRoadActor = Pair.Key;
		if (MetaRoadActor && Pair.Value)
		{
			// Inside the bake transaction → Modify() so Undo restores the previous LastGeneratedActor.
			MetaRoadActor->Modify();
			MetaRoadActor->LastGeneratedActor = Pair.Value;
			MetaRoadActor->MarkPackageDirty();
		}
	}

	return SourceToOutput.Num();
}

#undef LOCTEXT_NAMESPACE
