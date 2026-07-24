/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Utils/AssetUtils.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "InteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"

#include "AssetUtils/CreateStaticMeshUtil.h"
#include "Physics/ComponentCollisionUtil.h"

#include "ConversionUtils/DynamicMeshToVolume.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h" // transient static-mesh build (FBX From scratch)
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h" // disable collision cook on transient export meshes

#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Blueprint.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#include "ToolTargets/VolumeComponentToolTarget.h"  // for CVarModelingMaxVolumeTriangleCount
#include "Engine/BlockingVolume.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Model.h"
//#include "BSPOps.h"		// in UnrealEd
#include "Editor/EditorEngine.h"		// for FActorLabelUtilities

#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"
#include "ModelingModeAssetUtils.h"      // UE::Modeling::GetGlobalAssetRootPath / GetWorldRelativeAssetRootPath / AutoSaveAsset
#include "Kismet2/ComponentEditorUtils.h"

#include "EditorMode/MetaRoadBakeSettings.h"
#include "AssetRegistry/AssetRegistryModule.h" // FAssetRegistryModule::AssetCreated
#include "ContentBrowserModule.h"        // FContentBrowserModule
#include "IContentBrowserSingleton.h"    // IContentBrowserSingleton, FSaveAssetDialogConfig
#include "ContentBrowserItemPath.h"      // FContentBrowserItemPath
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProcess.h"

//extern UNREALED_API UEditorEngine* GEditor;

FString AssetUtils::GenerateValidComponentName(const FString& DesierName, AActor* ComponentOwner)
{
	check(ComponentOwner);

	FString ComponentTypeName = DesierName;

	// Strip off 'Component' if the class ends with that.  It just looks better in the UI.
	FString SuffixToStrip(TEXT("Component"));
	if (ComponentTypeName.EndsWith(SuffixToStrip))
	{
		ComponentTypeName.LeftInline(ComponentTypeName.Len() - SuffixToStrip.Len(), EAllowShrinking::No);
	}

	// Strip off 'Actor' if the class ends with that so as not to confuse actors with components
	SuffixToStrip = TEXT("Actor");
	if (ComponentTypeName.EndsWith(SuffixToStrip))
	{
		ComponentTypeName.LeftInline(ComponentTypeName.Len() - SuffixToStrip.Len(), EAllowShrinking::No);
	}

	// Try to create a name without any numerical suffix first
	int32 Counter = 1;
	FString ComponentInstanceName = ComponentTypeName;
	while (!FComponentEditorUtils::IsComponentNameAvailable(ComponentInstanceName, ComponentOwner))
	{
		// Assign the lowest possible numerical suffix
		ComponentInstanceName = FString::Printf(TEXT("%s%d"), *ComponentTypeName, Counter++);
	}

	return ComponentInstanceName;
}


FCreateMeshObjectResult AssetUtils::CreateMeshObject(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName, bool bTransient)
{
	FCreateMeshObjectResult ResultOut;

	if (CreateMeshParams.TypeHint == ECreateObjectTypeHint::DynamicMeshActor)
	{
		// DynamicMesh output lives in the level package (no standalone asset), so the transient flag has no effect.
		ResultOut = CreateDynamicMeshActor(MoveTemp(CreateMeshParams), Parent, DesierComponentName);
	}
	else if (CreateMeshParams.TypeHint == ECreateObjectTypeHint::StaticMesh)
	{
		ResultOut = CreateStaticMeshAsset(MoveTemp(CreateMeshParams), Parent, DesierComponentName, bTransient);
	}
	else
	{
		ResultOut.ResultCode = ECreateModelingObjectResult::Failed_Unknown;
	}
	return ResultOut;
}

FCreateMeshObjectResult AssetUtils::CreateDynamicMeshActor(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName)
{
	check(Parent);

	AActor* Actor = Parent->GetOwner();
	check(Actor);

	UDynamicMeshComponent* NewComponent = NewObject<UDynamicMeshComponent>(Parent, *GenerateValidComponentName(DesierComponentName, Actor), RF_Transactional);
	NewComponent->SetupAttachment(Parent);
	NewComponent->OnComponentCreated();
	Actor->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();
	NewComponent->ResetRelativeTransform();
	NewComponent->SetMobility(EComponentMobility::Static);


	// assume that DynamicMeshComponent always has tangents on it's internal UDynamicMesh
	NewComponent->SetTangentsType(EDynamicMeshComponentTangentsMode::ExternallyProvided);

	if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::DynamicMesh)
	{
		FDynamicMesh3 SetMesh = MoveTemp(CreateMeshParams.DynamicMesh.GetValue());
		if (SetMesh.IsCompact() == false)
		{
			SetMesh.CompactInPlace();
		}
		NewComponent->SetMesh(MoveTemp(SetMesh));
		NewComponent->NotifyMeshUpdated();
	}
	else if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::MeshDescription)
	{
		const FMeshDescription* MeshDescription = &CreateMeshParams.MeshDescription.GetValue();
		FDynamicMesh3 Mesh(UE::Geometry::EMeshComponents::FaceGroups);
		Mesh.EnableAttributes();
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(MeshDescription, Mesh, true);
		NewComponent->SetMesh(MoveTemp(Mesh));
	}
	else
	{
		return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_InvalidMesh };
	}

	//NewActor->SetActorTransform(CreateMeshParams.Transform);
	//FActorLabelUtilities::SetActorLabelUnique(NewActor, CreateMeshParams.BaseName);

	// set materials
	TArray<UMaterialInterface*> ComponentMaterials = CreateMeshParams.Materials;
	for (int32 k = 0; k < ComponentMaterials.Num(); ++k)
	{
		NewComponent->SetMaterial(k, ComponentMaterials[k]);
	}

	// configure collision
	if (CreateMeshParams.bEnableCollision)
	{
		if (CreateMeshParams.CollisionShapeSet.IsSet())
		{
			UE::Geometry::SetSimpleCollision(NewComponent, CreateMeshParams.CollisionShapeSet.GetPtrOrNull());
		}

		NewComponent->CollisionType = CreateMeshParams.CollisionMode;
		// enable complex collision so that raycasts can hit this object
		NewComponent->bEnableComplexCollision = true;

		// force collision update
		NewComponent->UpdateCollision(false);
	}

	// configure raytracing
	NewComponent->SetEnableRaytracing(CreateMeshParams.bEnableRaytracingSupport);

	Actor->PostEditChange();

	// emit result
	FCreateMeshObjectResult ResultOut;
	ResultOut.ResultCode = ECreateModelingObjectResult::Ok;
	ResultOut.NewActor = Actor;
	ResultOut.NewComponent = NewComponent;
	ResultOut.NewAsset = nullptr;
	return ResultOut;
}

namespace
{
	// Build a transient (RAM-only) UStaticMesh from the params' source mesh — used by the FBX "From scratch"
	// export so the layer geometry can be handed to FFbxExporter without creating any persistent content asset.
	UStaticMesh* BuildTransientStaticMeshFromParams(FCreateMeshObjectParams& Params)
	{
		using namespace UE::Geometry;

		FMeshDescription MeshDescription;
		FStaticMeshAttributes Attributes(MeshDescription);
		Attributes.Register();

		if (Params.MeshType == ECreateMeshObjectSourceMeshType::DynamicMesh)
		{
			const FDynamicMesh3& Mesh = Params.DynamicMesh.GetValue();
			FDynamicMeshToMeshDescription Converter;
			// Don't copy tangents — the road dynamic meshes have no tangent overlay, so copying yields zero
			// tangents/bi-normals (the build then warns). They're recomputed in the build below.
			Converter.Convert(&Mesh, MeshDescription, /*bCopyTangents=*/false);
		}
		else if (Params.MeshType == ECreateMeshObjectSourceMeshType::MeshDescription)
		{
			MeshDescription = Params.MeshDescription.GetValue();
		}
		else
		{
			return nullptr;
		}

		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		StaticMesh->InitResources();
		StaticMesh->SetLightingGuid();
		// Keep a CPU copy of the geometry (the FBX exporter reads it on a transient mesh) and skip streaming.
		StaticMesh->bAllowCPUAccess = true;
		StaticMesh->NeverStream = true;

		// One material slot per component material (matches the engine path's NumMaterialSlots = Materials.Num(),
		// which the caller relies on when stamping MaterialSlotName afterwards).
		for (UMaterialInterface* Material : Params.Materials)
		{
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
		}

		// Full editor build (NOT bFastBuild) via a source model — the fast path left zero RenderData bounds; this
		// computes proper bounds and recomputes a valid tangent basis (the source dynamic mesh has none).
		StaticMesh->SetNumSourceModels(1);
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
		SourceModel.BuildSettings.bRecomputeNormals = false;  // the converter wrote per-vertex normals
		SourceModel.BuildSettings.bRecomputeTangents = true;  // recompute → no near-zero tangent/bi-normal warnings
		SourceModel.BuildSettings.bUseMikkTSpace = true;
		SourceModel.BuildSettings.bGenerateLightmapUVs = false;
		SourceModel.BuildSettings.bRemoveDegenerates = false;

		FMeshDescription* SourceDesc = StaticMesh->CreateMeshDescription(0);
		*SourceDesc = MoveTemp(MeshDescription);
		StaticMesh->CommitMeshDescription(0);

		// Throwaway export mesh — it needs no physics. The road layers request CTF_UseComplexAsSimple collision, so
		// without this the engine tries to cook a tri-mesh from a transient mesh with no cooked/CPU collision data
		// and spams "GetPhysicsTriMeshData: CPU data not available" / "no source models". Skip the cook entirely.
		StaticMesh->CreateBodySetup();
		if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
		{
			BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
			BodySetup->bNeverNeedsCookedCollisionData = true;
		}

		StaticMesh->Build(/*bSilent=*/true);

		return StaticMesh;
	}
}

FCreateMeshObjectResult AssetUtils::CreateStaticMeshAsset(FCreateMeshObjectParams&& CreateMeshParams, USceneComponent* Parent, const FString& DesierComponentName, bool bTransient)
{
	UStaticMesh* NewStaticMesh = nullptr;

	if (bTransient)
	{
		// RAM-only path (FBX From scratch): build the mesh in the transient package, no disk asset / registry / autosave.
		NewStaticMesh = BuildTransientStaticMeshFromParams(CreateMeshParams);
		if (!NewStaticMesh)
		{
			return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_InvalidMesh };
		}
	}
	else
	{
		UE::AssetUtils::FStaticMeshAssetOptions AssetOptions;

		// The asset path is resolved entirely from UMetaRoadBakeSettings (see GetNewAssetPath → GetNewAssetPathName).
		{
			ECreateModelingObjectResult AssetPathResult = GetNewAssetPath(
				AssetOptions.NewAssetPath,
				CreateMeshParams.BaseName,
				nullptr,
				CreateMeshParams.TargetWorld);

			if (AssetPathResult != ECreateModelingObjectResult::Ok)
			{
				return FCreateMeshObjectResult{ AssetPathResult };
			}
		}

		AssetOptions.NumSourceModels = 1;
		AssetOptions.NumMaterialSlots = CreateMeshParams.Materials.Num();
		AssetOptions.AssetMaterials = (CreateMeshParams.AssetMaterials.Num() == AssetOptions.NumMaterialSlots) ? CreateMeshParams.AssetMaterials : CreateMeshParams.Materials;

		AssetOptions.bEnableRecomputeNormals = CreateMeshParams.bEnableRecomputeNormals;
		AssetOptions.bEnableRecomputeTangents = CreateMeshParams.bEnableRecomputeTangents;
		AssetOptions.bGenerateNaniteEnabledMesh = CreateMeshParams.bEnableNanite;
		AssetOptions.NaniteSettings = CreateMeshParams.NaniteSettings;
		AssetOptions.bGenerateLightmapUVs = CreateMeshParams.bGenerateLightmapUVs;

		AssetOptions.bCreatePhysicsBody = CreateMeshParams.bEnableCollision;
		AssetOptions.CollisionType = CreateMeshParams.CollisionMode;

		if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::DynamicMesh)
		{
			FDynamicMesh3* DynamicMesh = &CreateMeshParams.DynamicMesh.GetValue();
			AssetOptions.SourceMeshes.DynamicMeshes.Add(DynamicMesh);
		}
		else if (CreateMeshParams.MeshType == ECreateMeshObjectSourceMeshType::MeshDescription)
		{
			FMeshDescription* MeshDescription = &CreateMeshParams.MeshDescription.GetValue();
			AssetOptions.SourceMeshes.MoveMeshDescriptions.Add(MeshDescription);
		}
		else
		{
			return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_InvalidMesh };
		}

		UE::AssetUtils::FStaticMeshResults ResultData;
		UE::AssetUtils::ECreateStaticMeshResult AssetResult = UE::AssetUtils::CreateStaticMeshAsset(AssetOptions, ResultData);

		if (AssetResult != UE::AssetUtils::ECreateStaticMeshResult::Ok)
		{
			return FCreateMeshObjectResult{ ECreateModelingObjectResult::Failed_AssetCreationFailed };
		}

		NewStaticMesh = ResultData.StaticMesh;
	}


	check(Parent);
	AActor* Actor = Parent->GetOwner();
	check(Actor);

	UStaticMeshComponent* NewComponent = NewObject<UStaticMeshComponent>(Parent, *GenerateValidComponentName(DesierComponentName, Actor), RF_Transactional);
	NewComponent->SetupAttachment(Parent);
	NewComponent->OnComponentCreated();
	Actor->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();
	NewComponent->ResetRelativeTransform();
	NewComponent->SetMobility(EComponentMobility::Static);

	//return FCreateMeshObjectResult{ECreateModelingObjectResult::Failed_ActorCreationFailed};
	

	// this disconnects the component from various events
	NewComponent->UnregisterComponent();
	// replace the UStaticMesh in the component
	NewComponent->SetStaticMesh(NewStaticMesh);

	// set materials
	TArray<UMaterialInterface*> ComponentMaterials = CreateMeshParams.Materials;
	for (int32 k = 0; k < ComponentMaterials.Num(); ++k)
	{
		NewComponent->SetMaterial(k, ComponentMaterials[k]);
	}

	// set simple collision geometry
	if (CreateMeshParams.CollisionShapeSet.IsSet())
	{
		UE::Geometry::SetSimpleCollision(NewComponent, CreateMeshParams.CollisionShapeSet.GetPtrOrNull(),
			UE::Geometry::GetCollisionSettings(NewComponent));
	}

	// re-connect the component (?)
	NewComponent->RegisterComponent();

	NewStaticMesh->PostEditChange();

	// Post-creation handling per GenerationMode (mirrors UE::Modeling::OnNewAssetCreated): autosave for
	// AutoGenerateAndAutosave + InteractivePromptToSave (the user picked a path); otherwise register the
	// asset so it appears in the Content Browser (engine CreateStaticMeshAsset already marked the package
	// dirty). DynamicMesh output has no standalone asset (lives in the level package), so this StaticMesh
	// path is the only place the setting applies. Skipped entirely for transient (RAM-only) output.
	if (!bTransient)
	{
		switch (UMetaRoadBakeSettings::Get()->GenerationMode)
		{
		case EMetaRoadAssetGenerationBehavior::AutoGenerateAndAutosave:
		case EMetaRoadAssetGenerationBehavior::InteractivePromptToSave:
			UE::Modeling::AutoSaveAsset(NewStaticMesh);
			break;
		case EMetaRoadAssetGenerationBehavior::AutoGenerateButDoNotAutosave:
		default:
			NewStaticMesh->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(NewStaticMesh);
			break;
		}
	}

	NewComponent->RecreatePhysicsState();
	NewComponent->SetRelativeTransform(CreateMeshParams.Transform);

	// update transform
	//StaticMeshActor->SetActorTransform(CreateMeshParams.Transform);

	// emit result
	FCreateMeshObjectResult ResultOut;
	ResultOut.ResultCode = ECreateModelingObjectResult::Ok;
	ResultOut.NewActor = Actor;
	ResultOut.NewComponent = NewComponent;
	ResultOut.NewAsset = NewStaticMesh;
	return ResultOut;
}

ECreateModelingObjectResult AssetUtils::GetNewAssetPath(FString& OutNewAssetPath, const FString& BaseName, const UObject* StoreRelativeToObject, const UWorld* TargetWorld)
{
	FString RelativeToObjectFolder;
	if (StoreRelativeToObject != nullptr)
	{
		// find path to asset
		UPackage* AssetOuterPackage = CastChecked<UPackage>(StoreRelativeToObject->GetOuter());
		if (ensure(AssetOuterPackage))
		{
			FString AssetPackageName = AssetOuterPackage->GetName();
			RelativeToObjectFolder = FPackageName::GetLongPackagePath(AssetPackageName);
		}
	}
	else
	{
		if (!ensure(TargetWorld)) {
			return ECreateModelingObjectResult::Failed_InvalidWorld;
		}
	}

	OutNewAssetPath = AssetUtils::GetNewAssetPathName(BaseName, TargetWorld, RelativeToObjectFolder);
	if (OutNewAssetPath.Len() == 0)
	{
		// Empty path = the user cancelled the interactive save dialog (InteractivePromptToSave mode).
		return ECreateModelingObjectResult::Cancelled;
	}

	return ECreateModelingObjectResult::Ok;
}

namespace
{
	// Interactive-batch state (see AssetUtils::Begin/EndAssetGenerationBatch): while a bake runs in
	// InteractivePromptToSave mode, the user picks ONE output folder up front and it is reused for every
	// generated mesh instead of prompting per asset.
	bool GInAssetGenerationBatch = false;
	FString GBatchInteractiveFolder;

	// Ported from UE::Local (ModelingModeAssetUtils.cpp) — these content-browser helpers are not exported.
	FString GetActiveAssetFolderPath()
	{
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
		return CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : FString();
	}

	FString MakeUniqueAssetName(const FString& FolderPath, const FString& AssetBaseName)
	{
		FString UniquePackageName, UniqueAssetName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(FolderPath + TEXT("/") + AssetBaseName, TEXT(""), UniquePackageName, UniqueAssetName);
		return UniqueAssetName;
	}

	FString InteractiveSelectAssetPath(const FString& DefaultAssetName, const FText& DialogTitleMessage)
	{
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

		FString UseDefaultAssetName = DefaultAssetName;
		FString CurrentPath = GetActiveAssetFolderPath();
		if (CurrentPath.IsEmpty() == false)
		{
			UseDefaultAssetName = MakeUniqueAssetName(CurrentPath, DefaultAssetName);
		}

		FSaveAssetDialogConfig Config;
		Config.DefaultAssetName = UseDefaultAssetName;
		Config.DialogTitleOverride = DialogTitleMessage;
		Config.DefaultPath = CurrentPath;
		return ContentBrowser.CreateModalSaveAssetDialog(Config);
	}
}

FString AssetUtils::GetNewAssetPathName(const FString& BaseNameIn, const UWorld* TargetWorld, const FString& SuggestedFolder)
{
	// Port of UE::Modeling::GetNewAssetPathName, driven by UMetaRoadBakeSettings instead of the engine's
	// UModelingToolsEditorModeSettings (restrictive-mode branches dropped). Reuses the engine's exported
	// root-path + random-suffix helpers.
	const UMetaRoadBakeSettings* Settings = UMetaRoadBakeSettings::Get();

	FString PackageFolderPath = UE::Modeling::GetGlobalAssetRootPath();

	if (SuggestedFolder.Len() > 0 && SuggestedFolder.StartsWith(TEXT("/")))
	{
		// An absolute suggested folder (e.g. "store relative to object") overrides the settings.
		PackageFolderPath = SuggestedFolder;
	}
	else
	{
		if (Settings->GenerationLocation == EMetaRoadAssetGenerationLocation::WorldRelative)
		{
			PackageFolderPath = UE::Modeling::GetWorldRelativeAssetRootPath(TargetWorld);
		}

		// Unsaved level → GetWorldRelativeAssetRootPath returns "/Temp"; optionally redirect to the /Game root.
		if (PackageFolderPath.StartsWith(TEXT("/Temp")) && Settings->bStoreUnsavedLevelAssetsInTopLevelGameFolder)
		{
			PackageFolderPath = UE::Modeling::GetGlobalAssetRootPath();
		}

		// Append the fixed auto-generated path segment.
		if (Settings->AutoGeneratedAssetPath.Len() > 0)
		{
			PackageFolderPath = FPaths::Combine(PackageFolderPath, Settings->AutoGeneratedAssetPath);
		}

		// Append a per-user subfolder.
		if (Settings->bUsePerUserAutogenSubfolder)
		{
			FString UsernameString = Settings->AutogenSubfolderUserNameOverride.TrimStartAndEnd();
			if (UsernameString.IsEmpty())
			{
				UsernameString = FPlatformProcess::UserName();
			}
			if (!UsernameString.IsEmpty())
			{
				PackageFolderPath = FPaths::Combine(PackageFolderPath, UsernameString);
			}
		}

		// Use the currently-visible Content Browser folder if requested and available.
		if (Settings->GenerationLocation == EMetaRoadAssetGenerationLocation::CurrentAssetBrowserPath)
		{
			const FString CurrentAssetPath = GetActiveAssetFolderPath();
			if (CurrentAssetPath.IsEmpty() == false)
			{
				PackageFolderPath = CurrentAssetPath;
			}
		}
	}

	FString ObjectBaseName = BaseNameIn;

	if (GInAssetGenerationBatch && !GBatchInteractiveFolder.IsEmpty())
	{
		// Interactive batch: a single folder was chosen up front for the whole bake
		// (BeginAssetGenerationBatch) — reuse it for every mesh instead of prompting per asset.
		PackageFolderPath = GBatchInteractiveFolder;
	}
	else if (Settings->GenerationMode == EMetaRoadAssetGenerationBehavior::InteractivePromptToSave)
	{
		// Non-batch interactive: prompt for folder/name per asset; cancelling discards it (empty return).
		const FString SelectedPath = InteractiveSelectAssetPath(
			ObjectBaseName,
			NSLOCTEXT("MetaRoadAssetUtils", "GeneratePathDialogWarning", "Choose Folder Path and Name for New Asset. Cancel to Discard New Asset."));
		if (SelectedPath.IsEmpty() == false)
		{
			PackageFolderPath = FPaths::GetPath(SelectedPath);
			ObjectBaseName = FPaths::GetBaseFilename(SelectedPath, true);
		}
		else
		{
			return FString();
		}
	}

	FString UseBaseName = ObjectBaseName;

	if (Settings->bAppendRandomStringToName)
	{
		const FString GuidString = UE::Modeling::GenerateRandomShortHexString();
		UseBaseName = FString::Printf(TEXT("%s_%s"), *UseBaseName, *GuidString);
	}

	FString PackageNameOut, AssetNameOut;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(FPaths::Combine(PackageFolderPath, UseBaseName), TEXT(""), PackageNameOut, AssetNameOut);

	return PackageNameOut;
}

bool AssetUtils::BeginAssetGenerationBatch()
{
	GInAssetGenerationBatch = true;
	GBatchInteractiveFolder.Reset();

	// In InteractivePromptToSave mode a bake would otherwise pop a modal save dialog for every generated mesh
	// (one road splits into many sub-meshes). Prompt ONCE here for a single output folder and reuse it for the
	// whole batch (see GetNewAssetPathName). Cancelling aborts the bake (returns false) so nothing is generated.
	if (UMetaRoadBakeSettings::Get()->GenerationMode == EMetaRoadAssetGenerationBehavior::InteractivePromptToSave)
	{
		const FString Selected = InteractiveSelectAssetPath(
			TEXT("Road"),
			NSLOCTEXT("MetaRoadAssetUtils", "GenerateBatchPathDialog",
				"Choose a folder for the generated road mesh assets. Cancel to abort the bake."));
		if (Selected.IsEmpty())
		{
			GInAssetGenerationBatch = false;
			return false;
		}
		GBatchInteractiveFolder = FPaths::GetPath(Selected);
	}
	return true;
}

void AssetUtils::EndAssetGenerationBatch()
{
	GInAssetGenerationBatch = false;
	GBatchInteractiveFolder.Reset();
}

TSet<UClass*> AssetUtils::GetAllClassesOfSubClass(const UClass* BaseClass)
{
	// The body touches TObjectIterator, the AssetRegistry and loads assets — all of which
	// require the GameThread. When called from a worker thread, marshal the call onto the
	// GameThread (where IsInGameThread() is true, so the body runs) and block until done.
	if (!IsInGameThread())
	{
		const TSharedRef<TPromise<TSet<UClass*>>, ESPMode::ThreadSafe> Promise =
			MakeShared<TPromise<TSet<UClass*>>, ESPMode::ThreadSafe>();

		AsyncTask(ENamedThreads::GameThread, [BaseClass, Promise]()
		{
			Promise->SetValue(GetAllClassesOfSubClass(BaseClass));
		});

		return Promise->GetFuture().Get();
	}

	TSet<UClass*> OutClasses;

	auto NativeClassFilter = [BaseClass] (const UClass* Class) -> bool
	{
		return Class->IsChildOf(BaseClass) && Class->IsNative() &&
			!Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract) &&
			!Class->GetName().StartsWith(TEXT("SKEL_")) && !Class->GetName().StartsWith(TEXT("REINST_"));
	};

	auto BlueprintClassFilter = [BaseClass](const FAssetData & BlueprintClassData)
	{
		const FString NativeParentClassPath = BlueprintClassData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
		const FSoftClassPath ClassPath(NativeParentClassPath);

		UClass* NativeParentClass = ClassPath.ResolveClass();

		return NativeParentClass // Class may have been removed, or renamed and not correctly redirected
			&& (NativeParentClass == BaseClass || NativeParentClass->IsChildOf(BaseClass));;
	};



	// First search native classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (NativeClassFilter(Class))
		{
			OutClasses.Add(Class);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> BlueprintList;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintList, true);

	for (const FAssetData& AssetData : BlueprintList)
	{
		if (BlueprintClassFilter(AssetData))
		{
			// If there are a lot of assets, this may be slow.
			UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
			UClass* LoadedClass = BlueprintAsset->GeneratedClass;
			if (ensure(LoadedClass && BlueprintAsset->ParentClass))
			{
				OutClasses.Add(LoadedClass);
			}
		}

	}

	return OutClasses;

}