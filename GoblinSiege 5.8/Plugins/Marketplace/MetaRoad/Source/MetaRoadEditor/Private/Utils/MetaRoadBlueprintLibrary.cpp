/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/MetaRoadBlueprintLibrary.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadBlueprintLibrary)

#include "MetaRoadActor.h"
#include "RoadSplineComponent.h"
#include "MetaRoadSubsystem.h"
#include "MetaRoadModule.h" // LogMetaRoad
#include "EngineUtils.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "ScopedTransaction.h"
#include "Engine/Engine.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"

// Legacy build-preset migration
#include "Assets/RoadBuildPreset.h"
#include "Assets/RoadBuildPreset/RoadBuildPresetFactory.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "ToolPresetAsset.h"
#include "ToolPresetAssetSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "MetaRoadBlueprintLibrary"

TArray<AMetaRoad*> UMetaRoadBlueprintLibrary::ConvertActorsToMetaRoadActors(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return {};
	}

	TArray<AActor*> ToConvert;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor->IsA<AMetaRoad>() && Actor->FindComponentByClass<URoadSplineComponent>())
		{
			ToConvert.Add(Actor);
		}
	}

	if (ToConvert.IsEmpty())
	{
		return {};
	}

	const FScopedTransaction Transaction(LOCTEXT("ConvertToMetaRoadActor", "Convert Actors to MetaRoadActor"));

	for (AActor* OldActor : ToConvert)
	{
		OldActor->ForEachComponent<URoadSplineComponent>(true, [&](URoadSplineComponent* Comp)
		{
			Comp->RefreshConnectionGuids();
		});
	}

	TArray<AMetaRoad*> Result;
	TArray<URoadSplineComponent*> MovedSplines;
	for (AActor* OldActor : ToConvert)
	{
		OldActor->Modify();

		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = OldActor->GetLevel();
		AMetaRoad* NewActor = World->SpawnActor<AMetaRoad>(AMetaRoad::StaticClass(), OldActor->GetActorTransform(), SpawnParams);

		if (!NewActor)
		{
			continue;
		}
		NewActor->Modify();

		// Transfer all instance components (URoadSplineComponent and any others)
		USceneComponent* OldRoot = OldActor->GetRootComponent();
		const TArray<UActorComponent*> Components = OldActor->GetInstanceComponents();
		for (UActorComponent* Comp : Components)
		{
			if (Comp == OldRoot)
				continue;  // NewActor already has its own DSO root; skip OldActor's root
			Comp->Modify();
			if (USceneComponent* SC = Cast<USceneComponent>(Comp))
			{
				SC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
			OldActor->RemoveInstanceComponent(Comp);
			Comp->Rename(nullptr, NewActor); // ULaneConnection GUIDs survive Rename
			if (USceneComponent* SC = Cast<USceneComponent>(Comp))
			{
				SC->AttachToComponent(NewActor->GetRootComponent(),FAttachmentTransformRules::KeepWorldTransform);
			}
			NewActor->AddInstanceComponent(Comp);
			if (URoadSplineComponent* SplineComp = Cast<URoadSplineComponent>(Comp))
			{
				MovedSplines.Add(SplineComp);
			}
		}

		// Duplicate Blueprint SCS components — GetInstanceComponents() skips them
		{
			const TSet<UActorComponent*> InstanceSet(Components);
			TArray<URoadSplineComponent*> AllSplines;
			OldActor->GetComponents<URoadSplineComponent>(AllSplines);
			for (URoadSplineComponent* Comp : AllSplines)
			{
				if (InstanceSet.Contains(Comp))
					continue;  // already transferred as instance component above

				Comp->ConnectionGuidSnapshot.Reset();
				URoadSplineComponent* DupComp = DuplicateObject<URoadSplineComponent>(Comp, NewActor);
				DupComp->RegisterComponent();
				DupComp->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				NewActor->AddInstanceComponent(DupComp);
				MovedSplines.Add(DupComp);
			}
		}

		FActorLabelUtilities::SetActorLabelUnique(NewActor, OldActor->GetActorLabel());
		NewActor->SetFolderPath(OldActor->GetFolderPath());

		// Flag the new actor's package for saving. In One-File-Per-Actor worlds this
		// is the actor's own external package; otherwise it is the level package.
		NewActor->MarkPackageDirty();

		// Use EditorDestroyActor so the owning level is modified (dirtied + recorded
		// for undo). Raw World->DestroyActor leaves the level unflagged, so removing
		// the old actor would not be persisted on the next save.
		World->EditorDestroyActor(OldActor, /*bShouldModifyLevel=*/true);
		Result.Add(NewActor);
	}

	// Restore connections for moved splines; GUID map covers the entire world
	// so cross-actor connections are found even if the target spline wasn't moved.
	UMetaRoadSubsystem::EndCopySplineTransaction(MovedSplines);

	if (GEditor)
	{
		GEditor->NoteSelectionChange();
		GEditor->RedrawLevelEditingViewports();
	}

	return Result;
}

TArray<URoadBuildPreset*> UMetaRoadBlueprintLibrary::MigrateLegacyBuildPresets(const FString& FallbackPath)
{
	static const FString LegacyKey = TEXT("TriangulateRoadTool"); // old UTriangulateRoadTool preset key

	TArray<URoadBuildPreset*> Created;
	int32 Skipped = 0;

	// Gather the engine preset collections to scan: the default collection + every collection asset.
	TArray<UInteractiveToolsPresetCollectionAsset*> Collections;
	if (GEditor)
	{
		if (UToolPresetAssetSubsystem* Subsystem = GEditor->GetEditorSubsystem<UToolPresetAssetSubsystem>())
		{
			if (UInteractiveToolsPresetCollectionAsset* Default = Subsystem->GetDefaultCollection())
			{
				Collections.Add(Default);
			}
		}
	}
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FARFilter Filter;
		Filter.ClassPaths.Add(UInteractiveToolsPresetCollectionAsset::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> CollectionAssets;
		AssetRegistryModule.Get().GetAssets(Filter, CollectionAssets);
		for (const FAssetData& AssetData : CollectionAssets)
		{
			if (UInteractiveToolsPresetCollectionAsset* Collection = Cast<UInteractiveToolsPresetCollectionAsset>(AssetData.GetAsset()))
			{
				Collections.AddUnique(Collection);
			}
		}
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	for (UInteractiveToolsPresetCollectionAsset* Collection : Collections)
	{
		FInteractiveToolPresetStore* Store = Collection ? Collection->PerToolPresets.Find(LegacyKey) : nullptr;
		if (!Store)
		{
			continue;
		}

		// New assets go next to the source collection asset (or FallbackPath for the engine default
		// collection, which isn't a /Game asset).
		FString DestDir = FallbackPath;
		const FString CollectionPackage = Collection->GetPackage()->GetName();
		if (CollectionPackage.StartsWith(TEXT("/Game")))
		{
			DestDir = FPackageName::GetLongPackagePath(CollectionPackage);
		}

		for (FInteractiveToolPresetDefinition& Definition : Store->NamedPresets)
		{
			// Load the stored property values into a fresh build-settings holder.
			UMetaRoadBuildSettings* Holder = NewObject<UMetaRoadBuildSettings>(GetTransientPackage());
			Holder->EnsureDefaultPropertySets();
			TArray<UObject*> PropertyObjects;
			for (const TObjectPtr<UMetaRoadBuildSettingsBase>& PropertySet : Holder->PropertySets)
			{
				if (PropertySet)
				{
					PropertyObjects.Add(PropertySet);
				}
			}
			Definition.LoadStoredPropertyData(PropertyObjects);

			const FString BaseName = ObjectTools::SanitizeObjectName(Definition.Label + TEXT("_Migrated"));
			FString PackageName, AssetName;
			AssetTools.CreateUniqueAssetName(DestDir / BaseName, TEXT(""), PackageName, AssetName);

			URoadBuildPresetFactory* Factory = NewObject<URoadBuildPresetFactory>();
			UObject* NewAsset = AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), URoadBuildPreset::StaticClass(), Factory);
			if (URoadBuildPreset* Preset = Cast<URoadBuildPreset>(NewAsset))
			{
				Preset->Settings = DuplicateObject<UMetaRoadBuildSettings>(Holder, Preset);
				Preset->DisplayName = FText::FromString(Definition.Label);
				Preset->Description = FText::FromString(Definition.Tooltip);
				Preset->MarkPackageDirty();
				Created.Add(Preset);
				UE_LOG(LogMetaRoad, Display, TEXT("MigrateLegacyBuildPresets: '%s' from '%s' -> '%s'"),
					*Definition.Label, *Collection->GetName(), *Preset->GetPathName());
			}
			else
			{
				++Skipped;
				UE_LOG(LogMetaRoad, Warning, TEXT("MigrateLegacyBuildPresets: failed to create asset for preset '%s' (collection '%s')"),
					*Definition.Label, *Collection->GetName());
			}
		}
	}

	UE_LOG(LogMetaRoad, Display, TEXT("MigrateLegacyBuildPresets: created %d preset(s), skipped %d."), Created.Num(), Skipped);
	return Created;
}

void UMetaRoadBlueprintLibrary::SaveAsTemplate(AMetaRoad* Road)
{
	if (!Road)
	{
		return;
	}

	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DefaultPath = TEXT("/Game");
	SaveConfig.DefaultAssetName = TEXT("BP_") + Road->GetActorLabel();
	SaveConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
	SaveConfig.DialogTitleOverride = NSLOCTEXT("MetaRoadBlueprintLibrary", "SaveAsTemplate_Title", "Save Road as Blueprint Template");

	const FString SaveObjectPath = IContentBrowserSingleton::Get().CreateModalSaveAssetDialog(SaveConfig);
	if (SaveObjectPath.IsEmpty())
	{
		return;
	}

	// Snapshot connection GUIDs so inter-spline links survive the actor -> Blueprint duplication.
	Road->ForEachComponent<URoadSplineComponent>(true, [](URoadSplineComponent* Comp)
	{
		Comp->RefreshConnectionGuids();
		Comp->CaptureConnectionGuids();
	});

	FKismetEditorUtilities::FCreateBlueprintFromActorParams Params;
	Params.bReplaceActor = false;
	Params.bOpenBlueprint = true;
	FKismetEditorUtilities::CreateBlueprintFromActor(
		FPackageName::ObjectPathToPackageName(SaveObjectPath), Road, Params);

	Road->ForEachComponent<URoadSplineComponent>(true, [](URoadSplineComponent* Comp)
	{
		Comp->ConnectionGuidSnapshot.Reset();
	});
}

#undef LOCTEXT_NAMESPACE
