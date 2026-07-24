/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "ModelingObjectsCreationAPI.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/AssetManager.h"

namespace AssetUtils
{
	METAROADEDITOR_API FString GenerateValidComponentName(const FString& DesierName, AActor* ComponentOwner);

	// bTransient: build the UStaticMesh in the transient package (RAM only — no disk asset, no asset-registry
	// entry, no autosave, no UMetaRoadBakeSettings path resolution). Used by the FBX "From scratch" export.
	METAROADEDITOR_API FCreateMeshObjectResult CreateMeshObject(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName, bool bTransient = false);

	METAROADEDITOR_API FCreateMeshObjectResult CreateStaticMeshAsset(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName, bool bTransient = false);
	METAROADEDITOR_API FCreateMeshObjectResult CreateDynamicMeshActor(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName);

	METAROADEDITOR_API ECreateModelingObjectResult GetNewAssetPath(FString& OutNewAssetPath, const FString& BaseName, const UObject* StoreRelativeToObject, const UWorld* World);

	// Resolves the package path + unique name for a new generated asset, reading UMetaRoadBakeSettings
	// (location mode, auto-gen path, per-user subfolder, random suffix, interactive save). Ported from UE's
	// UE::Modeling::GetNewAssetPathName but driven by the plugin's own settings. Returns an empty string if
	// the user cancels the interactive save dialog. SuggestedFolder (if absolute) overrides the settings.
	METAROADEDITOR_API FString GetNewAssetPathName(const FString& BaseName, const UWorld* TargetWorld, const FString& SuggestedFolder = FString());

	// Bracket a multi-asset generation pass (e.g. a Bake). In InteractivePromptToSave mode this prompts ONCE
	// for an output folder reused by every asset in the pass (instead of a modal dialog per mesh); returns
	// false if the user cancels that prompt (caller should abort without generating). No-op for other modes.
	// Always pair with EndAssetGenerationBatch().
	METAROADEDITOR_API bool BeginAssetGenerationBatch();
	METAROADEDITOR_API void EndAssetGenerationBatch();

	METAROADEDITOR_API TSet<UClass*> GetAllClassesOfSubClass(const UClass* BaseClass);

	/*
	template<typename T>
	void ForEachAsset(TFunctionRef<void(const T*, const FAssetData*)> Preset)
	{
		auto ForEachPresetInner = [&Preset]()
		{
			for (TObjectIterator<UClass> Itr; Itr; ++Itr)
			{
				if (Itr->IsChildOf(T::StaticClass()))
				{
					if (auto* DefaultObject = Cast<const T>(Itr->GetDefaultObject()))
					{
						if (DefaultObject->CanBeAdded())
						{
							Preset(Cast<const T>(Itr->GetDefaultObject()), nullptr);
						}
					}

				}
			}

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> AssetData;
			FARFilter Filter;
			Filter.ClassPaths.Add(T::StaticClass()->GetClassPathName());
			Filter.PackagePaths.Append({ FName("/Game"), FName("/MetaRoad")});
			Filter.bRecursiveClasses = true;
			Filter.bRecursivePaths = true;
			Filter.bIncludeOnlyOnDiskAssets = false;
			AssetRegistryModule.Get().GetAssets(Filter, AssetData);

			for (auto& It: AssetData)
			{
				if (T* Object = Cast<T>(It.GetAsset()))
				{
					Preset(Object, &It);
				}
			}
		};

		if (IsInGameThread())
		{
			ForEachPresetInner();
		}
		else
		{
			const TSharedRef<TPromise<void>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<void>());
			AsyncTask(ENamedThreads::GameThread, [&ForEachPresetInner, Promise]()
			{
				ForEachPresetInner();
				Promise->SetValue();
			});
			Promise->GetFuture().Get();
		}
		
	}
	*/
};
