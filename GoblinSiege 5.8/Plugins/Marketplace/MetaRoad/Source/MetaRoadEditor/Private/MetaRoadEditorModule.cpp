/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "MetaRoadEditorModule.h"
#include "Misc/EngineVersionComparison.h" // UE_VERSION_NEWER_THAN_OR_EQUAL
#include "MetaRoadEditorStyle.h"
#include "RoadSplineComponent.h"
#include "ComponentVisualizers/RoadSplineComponentVisualizer.h"
#include "ComponentVisualizers/RoadSectionComponentVisualizer.h"
#include "ComponentVisualizers/RoadOffsetComponentVisualizer.h"
#include "ComponentVisualizers/RoadWidthComponentVisualizer.h"
#include "ComponentVisualizers/RoadAttributeComponentVisualizer.h"
#include "EditorMode/MetaRoadDragDropHandler.h"
#include "EditorMode/MetaRoadSelectionModel.h"
#include "Editor.h"
#include "LevelEditorDragDropHandler.h"
#include "DetailCustomizations/RoadSplineComponentDetails.h"
#include "DetailCustomizations/MetaRoadActorDetails.h"
#include "MetaRoadActor.h"
#include "DetailCustomizations/RoadZoneTypeDetails.h"
#include "DetailCustomizations/RoadLaneAttributeProfileDetails.h"
#include "RoadEditorCommands.h"
#include "LevelEditor.h"
#include "Features/IModularFeatures.h"
#include "UnrealEdGlobals.h"
#include "Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorModeRegistry.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "EditorModeManager.h"
#include "MetaRoadModule.h"
#include "MetaRoadSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RoadSceneProxy/RoadSceneProxy.h"
#include "EngineUtils.h"
#include "MetaRoadEditorSettings.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "RoadMeshBuild/IRoadMeshBuildHost.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/RoadComputePipeline.h"
#include "RoadMeshBuild/RoadComputeFactoryRegistry.h"
#include "RoadMeshBuild/GenericDataBackgroundCompute.h"
#include "RoadMeshBuild/RoadGraphBackgroundCompute.h"
#include "RoadMeshBuild/ToolPropertySets.h"
#include "RoadMeshBuild/Ops/TriangulateRoadOp.h"
#include "Utils/AssetUtils.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Assets/RoadMarkProfileEditor/RoadMarkProfileThumbnailRenderer.h"
#include "Assets/RoadProfileEditor/RoadProfileThumbnailRenderer.h"
#include "Assets/RoadProfile.h"
#include "Assets/RoadMarkProfile.h"
#include "Assets/RoadPolygonProfile.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#if METAROAD_PRO
#include "Assets/RoadPolygonProfileEditor/RoadPolygonProfileThumbnailRenderer.h"
#include "Assets/LoftingDescriptorEditor/LoftingDescriptorThumbnailRenderer.h"
#include "Assets/RoadLaneAttributeLofting.h"
#endif
#include "Widgets/Docking/SDockTab.h"

#if METAROAD_PRO
#include "ComponentVisualizers/CrosswalkComponentVisualizer.h"
#include "CrosswalkComponent.h"
#include "ZoneGraphData.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphSubsystem.h"
#include "MetaRoadLandscapeSubsystem.h"
#endif


#define LOCTEXT_NAMESPACE "FMetaRoadEditorModule"

bool FMetaRoadEditorModule::bIsTileRendersVisibleInEditor = true;
bool FMetaRoadEditorModule::bIsEditorModeActive = false;
bool FMetaRoadEditorModule::bIsPreviewMode = false;
bool FMetaRoadEditorModule::bAnyRoadSplineSelected = false;

static TSharedPtr<FComponentVisualizer> GCrosswalkVisualizer;

namespace
{
	// FCoreDelegates::OnPostEngineInit was deprecated in UE 5.8 in favour of GetOnPostEngineInit();
	// keep 5.6/5.7 building against the old member.
	FSimpleMulticastDelegate& GetPostEngineInitDelegate()
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		return FCoreDelegates::GetOnPostEngineInit();
#else
		return FCoreDelegates::OnPostEngineInit;
#endif
	}
}

class FSplineComponentDetailsFix : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FSplineComponentDetailsFix);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		for (auto& It : ObjectsBeingCustomized)
		{
			if (URoadSplineComponent* Comp = Cast<URoadSplineComponent>(It))
			{
				return;
			}
		}

		OriginDetailCustomization = OriginalSplineDetails.DetailLayoutDelegate.Execute();
		OriginDetailCustomization->CustomizeDetails(DetailBuilder);
		
	}

	static FDetailLayoutCallback OriginalSplineDetails;

	TSharedPtr<IDetailCustomization>  OriginDetailCustomization;
};

FDetailLayoutCallback FSplineComponentDetailsFix::OriginalSplineDetails;

const FName FMetaRoadEditorModule::AssetCategoryName = TEXT("MetaRoadCategory");

void FMetaRoadEditorModule::StartupModule()
{
	FMetaRoadEditorStyle::Register();

	RegisterDetailCustomizations();

	FRoadEditorCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	// NOTE: the road editing UI used to live on a global Level-Editor toolbar combo button
	// (FMetaRoadEditorModule::AddToolbarExtension). It now lives inside UMetaRoadEditorMode's
	// toolkit, so the global extender is no longer registered here.

	FRoadComputeFactoryRegistry::Get().RegisterBuiltinFactories();

	BindRuntimeDelegates();

	GetPostEngineInitDelegate().AddRaw(this, &FMetaRoadEditorModule::OnPostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FMetaRoadEditorModule::OnPreExit);

#if METAROAD_PRO
	BindZoneGraphDelegates();
#endif

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAdvancedAssetCategory(AssetCategoryName, LOCTEXT("MetaRoadCategory", "Meta Road"));

	RegisterThumbnailRenderers();

	// If the active Attribute sub-mode's descriptor asset is destroyed, fall back to the Spline sub-mode
	// so the editor stops referencing a dead descriptor.
	OnAttributeBeginDestroydHandle = URoadLaneAttributeDescriptor::OnAttributeBeginDestroy.AddLambda([this](URoadLaneAttributeDescriptor* Attribute)
	{
		if (!bIsExiting)
		{
			if (auto* Class = FMetaRoadSelectionController::Get().GetSelectedAttributeDescriptor().Get())
			{
				if (Class->GetDefaultObject() == Attribute)
				{
					FMetaRoadSelectionController::Get().SetSplineEditorMode();
				}
			}
		}
	});
}

void FMetaRoadEditorModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());

#if METAROAD_PRO
		GUnrealEd->UnregisterComponentVisualizer(UCrosswalkComponent::StaticClass()->GetFName());
#endif
	}
	GCrosswalkVisualizer.Reset();
	FRoadComputeFactoryRegistry::Get().Reset();

	UnbindRuntimeDelegates();

	GetPostEngineInitDelegate().RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

#if METAROAD_PRO
	UnbindZoneGraphDelegates();
#endif

	URoadLaneAttributeDescriptor::OnAttributeBeginDestroy.Remove(OnAttributeBeginDestroydHandle);

	UnregisterDetailCustomizations();
}

void FMetaRoadEditorModule::RegisterDetailCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("RoadSplineComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FRoadSplineComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(AMetaRoad::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaRoadActorDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UMetaRoadBuildSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMetaRoadBuildSettingsDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("RoadZoneType"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRoadZoneTypeCustomization::MakeInstance));

	// The engine-type overrides ("SplineComponent" replacement + "RoadLaneAttributeProfile") are registered
	// later in OnPostEngineInit — they need the engine's own layouts to already be present.
	// UnregisterDetailCustomizations() tears down BOTH batches.
}

void FMetaRoadEditorModule::UnregisterDetailCustomizations()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Registered in RegisterDetailCustomizations().
	PropertyModule.UnregisterCustomClassLayout("RoadSplineComponent");
	PropertyModule.UnregisterCustomClassLayout(AMetaRoad::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomClassLayout(UMetaRoadBuildSettings::StaticClass()->GetFName());
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("RoadZoneType"));

	// Registered in OnPostEngineInit().
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("RoadLaneAttributeProfile"));
	PropertyModule.UnregisterCustomClassLayout("SplineComponent");
	// Restore the engine's original SplineComponent layout that OnPostEngineInit replaced with ours.
	if (FSplineComponentDetailsFix::OriginalSplineDetails.DetailLayoutDelegate.IsBound())
	{
		PropertyModule.RegisterCustomClassLayout("SplineComponent", FSplineComponentDetailsFix::OriginalSplineDetails.DetailLayoutDelegate);
	}
}

void FMetaRoadEditorModule::RegisterThumbnailRenderers()
{
	// Registered by UClass and kept for the editor's lifetime — no explicit unregister needed
	// (UThumbnailManager owns them; the renderer classes are GC objects, not dangling native pointers).
	UThumbnailManager::Get().RegisterCustomRenderer(URoadMarkProfile::StaticClass(), URoadMarkProfileThumbnailRenderer::StaticClass());
#if METAROAD_PRO
	UThumbnailManager::Get().RegisterCustomRenderer(URoadPolygonProfile::StaticClass(), URoadPolygonProfileThumbnailRenderer::StaticClass());
	UThumbnailManager::Get().RegisterCustomRenderer(URoadLaneAttributeLoftingDescriptor::StaticClass(), ULoftingDescriptorThumbnailRenderer::StaticClass());
#endif
	UThumbnailManager::Get().RegisterCustomRenderer(URoadProfile::StaticClass(), URoadProfileThumbnailRenderer::StaticClass());
}

void FMetaRoadEditorModule::BindRuntimeDelegates()
{
	// The schematic road-spline scene proxy lives in this editor module; bind the runtime factory delegates
	// so URoadSplineComponent (runtime) can create it / report its materials without referencing editor types.
	FMetaRoadDelegates::CreateRoadSplineSceneProxyDelegate.BindLambda(
		[](URoadSplineComponent* Component) -> FPrimitiveSceneProxy*
		{
			return new FRoadSplineSceneProxy(Component);
		});
	FMetaRoadDelegates::GetRoadSplineUsedMaterialsDelegate.BindLambda(
		[](const URoadSplineComponent* /*Spline*/, TArray<UMaterialInterface*>& OutMaterials)
		{
			for (const auto& It : GetDefault<UMetaRoadSettings>()->RoadZoneTypes)
			{
				OutMaterials.Add(It.Value.GetEditorMaterial());
			}
			const UMetaRoadEditorSettings* Settings = GetDefault<UMetaRoadEditorSettings>();
			OutMaterials.Add(Settings->SplineArrowMatrtial.Get());
			OutMaterials.Add(Settings->SelectedLaneMatrtial.Get());
			OutMaterials.Add(Settings->HiddenLaneMatrtial.Get());
			OutMaterials.Add(Settings->EmptyLaneMatrtial.Get());
		});
}

void FMetaRoadEditorModule::UnbindRuntimeDelegates()
{
	FMetaRoadDelegates::CreateRoadSplineSceneProxyDelegate.Unbind();
	FMetaRoadDelegates::GetRoadSplineUsedMaterialsDelegate.Unbind();
}

void FMetaRoadEditorModule::OnPostEngineInit()
{
	// Register the default road-spline visualizer (FRoadSplineComponentVisualizer) persistently — it stays
	// active even OUTSIDE the MetaRoad editor mode, so a selected road spline is always editable at the
	// spline-point level. The MetaRoad mode swaps it to sub-mode visualizers while active and restores it on
	// exit; the crosswalk visualizer stays mode-gated (see Activate/DeactivateEditing).
	FMetaRoadSelectionController::Get().RegisterDefaultVisualizer();

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	if (const FDetailLayoutCallback* SplineDetails = PropertyModule.GetClassNameToDetailLayoutNameMap().Find("SplineComponent"))
	{
		FSplineComponentDetailsFix::OriginalSplineDetails = *SplineDetails;
	}
	// Replace the engine's SplineComponent layout with our fix (its original is saved above so
	// UnregisterDetailCustomizations() can restore it). This override + RoadLaneAttributeProfile are
	// registered here (not at startup) because they need engine-registered layouts to already exist.
	PropertyModule.UnregisterCustomClassLayout("SplineComponent");
	PropertyModule.RegisterCustomClassLayout("SplineComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FSplineComponentDetailsFix::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("RoadLaneAttributeProfile", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRoadLaneAttributeProfileDetails::MakeInstance));

	// Route viewport asset drops through our handler so attribute/profile assets can be dropped onto
	// the selected RoadLane (see UMetaRoadLevelEditorDragDropHandler). The handler chains to the
	// engine default for any other asset.
	if (GEditor)
	{
		if (!GEditor->OnCreateLevelEditorDragDropHandler().IsBound())
		{
			GEditor->OnCreateLevelEditorDragDropHandler().BindLambda([]() -> ULevelEditorDragDropHandler*
			{
				return NewObject<UMetaRoadLevelEditorDragDropHandler>(GEditor);
			});
		}
		else
		{
			UE_LOG(LogMetaRoad, Warning,
				TEXT("OnCreateLevelEditorDragDropHandler is already bound; MetaRoad drag-and-drop of attribute assets is disabled."));
		}
	}

	// Track whether any road spline is selected (drives FRoadSplineSceneProxy's dimming of unselected roads).
	// Event-driven via the editor selection-changed signal; seed the initial value once.
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(this, &FMetaRoadEditorModule::OnEditorSelectionChanged);
	OnEditorSelectionChanged(nullptr);
}

void FMetaRoadEditorModule::OnEditorSelectionChanged(UObject* /*NewSelection*/)
{
	bool bAnySelected = false;
	if (GEditor)
	{
		for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
		{
			if (Cast<URoadSplineComponent>(*It))
			{
				bAnySelected = true;
				break;
			}
		}
	}
	bAnyRoadSplineSelected = bAnySelected;
}

void FMetaRoadEditorModule::OnPreExit()
{
	bIsExiting = true;

	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
	SelectionChangedHandle.Reset();

	if (GEditor)
	{
		GEditor->OnCreateLevelEditorDragDropHandler().Unbind();
	}

	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(URoadSplineComponent::StaticClass()->GetFName());

#if METAROAD_PRO
		GUnrealEd->UnregisterComponentVisualizer(UCrosswalkComponent::StaticClass()->GetFName());
#endif
	}
	FMetaRoadSelectionController::Get().EndSelection();
	GCrosswalkVisualizer.Reset();
}

void FMetaRoadEditorModule::ActivateEditing()
{
	if (!GUnrealEd)
	{
		return;
	}

	// The road-spline visualizer is already registered (default Spline visualizer, persistent since startup) —
	// just ensure it's active; the mode adds the crosswalk visualizer on top.
	FMetaRoadSelectionController::Get().RegisterDefaultVisualizer();

#if METAROAD_PRO
	if (!GCrosswalkVisualizer.IsValid())
	{
		GCrosswalkVisualizer = MakeShared<FCrosswalkComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UCrosswalkComponent::StaticClass()->GetFName(), GCrosswalkVisualizer);
		GCrosswalkVisualizer->OnRegister();
	}
#endif
}

void FMetaRoadEditorModule::DeactivateEditing()
{
	if (!GUnrealEd)
	{
		return;
	}

	// Leaving the mode does NOT unregister the road-spline visualizer — it stays active outside the mode.
	// Restore the default Spline sub-mode (the mode may have swapped it to Section/Offset/Width/Attribute).
	FMetaRoadSelectionController::Get().RegisterDefaultVisualizer();

#if METAROAD_PRO
	GUnrealEd->UnregisterComponentVisualizer(UCrosswalkComponent::StaticClass()->GetFName());
	GCrosswalkVisualizer.Reset();
#endif

	// Refresh the Details panel so the spline customization re-dispatches for the (now default Spline) sub-mode.
	FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor").NotifyCustomizationModuleChanged();
	if (GEditor)
	{
		GEditor->NoteSelectionChange();
	}
}

#if METAROAD_PRO

void FMetaRoadEditorModule::BindZoneGraphDelegates()
{
	OnRoadGraphRegistredHandle = URoadGraphDataComponent::OnComponentRegistredDelegate.AddLambda([this](URoadGraphDataComponent* RoadGraph)
	{
		ZoneGraphBuilder.RegisterZoneShapeComponent(*RoadGraph);
	});

	OnRoadGraphUnregistredHandle = URoadGraphDataComponent::OnComponentUnregistredDelegate.AddLambda([this](URoadGraphDataComponent* RoadGraph)
	{
		ZoneGraphBuilder.UnregisterZoneShapeComponent(*RoadGraph);
	});

	OnZoneGraphRequestRebuildHandle = UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.AddRaw(this, &FMetaRoadEditorModule::OnZoneGraphRequestRebuild);
	OnZoneGraphDataBuildDoneHandle = UE::ZoneGraphDelegates::OnZoneGraphDataBuildDone.AddRaw(this, &FMetaRoadEditorModule::OnZoneGraphDataBuildDone);

	// Bridge the runtime landscape-update request to the editor-only landscape subsystem.
	FMetaRoadDelegates::RequestLandscapeUpdateDelegate.BindStatic(&UMetaRoadLandscapeSubsystem::HandleSplineLandscapeUpdateRequest);
}

void FMetaRoadEditorModule::UnbindZoneGraphDelegates()
{
	UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.Remove(OnZoneGraphRequestRebuildHandle);
	UE::ZoneGraphDelegates::OnZoneGraphDataBuildDone.Remove(OnZoneGraphDataBuildDoneHandle);

	URoadGraphDataComponent::OnComponentRegistredDelegate.Remove(OnRoadGraphRegistredHandle);
	URoadGraphDataComponent::OnComponentUnregistredDelegate.Remove(OnRoadGraphUnregistredHandle);

	FMetaRoadDelegates::RequestLandscapeUpdateDelegate.Unbind();
}

void FMetaRoadEditorModule::OnZoneGraphRequestRebuild()
{
}

void FMetaRoadEditorModule::OnZoneGraphDataBuildDone(const FZoneGraphBuildData& /*BuildData*/)
{
	// Prevent recursive call to OnZoneGraphDataBuildDone
	if (bPreventGraphDataBuildDone)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return;
	}

	UZoneGraphSubsystem* ZoneGraphSubsystem = World->GetSubsystem<UZoneGraphSubsystem>();
	if (!ZoneGraphSubsystem)
	{
		return;
	}

	auto RegisteredZoneGraphData = ZoneGraphSubsystem->GetRegisteredZoneGraphData();

	TSet<ULevel*> SupportedLevels;

	// Find all Levels with URoadSplineComponent components
	for (auto& It : ZoneGraphBuilder.GetRegisteredZoneShapeComponents())
	{
		if (It.Component.IsValid())
		{
			if (ULevel* OwningLevel = It.Component->GetComponentLevel())
			{
				SupportedLevels.Add(OwningLevel);
			}
		}
	}

	// Remove worlds which already has data.
	for (const FRegisteredZoneGraphData& RegisteredData : RegisteredZoneGraphData)
	{
		if (RegisteredData.ZoneGraphData)
		{
			if (const ULevel* Level = RegisteredData.ZoneGraphData->GetLevel())
			{
				SupportedLevels.Remove(Level);
			}
		}
	}

	// Create new data for missing worlds.
	for (ULevel* Level : SupportedLevels)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = Level;
		World->SpawnActor<AZoneGraphData>(AZoneGraphData::StaticClass(), SpawnInfo);
	}

	// After creating AZoneGraphData, they are automatically registered in the ZoneGraphSubsystem, so we update the list
	RegisteredZoneGraphData = ZoneGraphSubsystem->GetRegisteredZoneGraphData();
	if (RegisteredZoneGraphData.Num() == 0)
	{
		return;
	}

	TArray<AZoneGraphData*> AllZoneGraphData;
	AllZoneGraphData.Reserve(RegisteredZoneGraphData.Num());

	for (const FRegisteredZoneGraphData& RegisteredData : RegisteredZoneGraphData)
	{
		if (RegisteredData.ZoneGraphData)
		{
			AllZoneGraphData.Add(RegisteredData.ZoneGraphData);
		}
	}

	ZoneGraphBuilder.BuildAll(AllZoneGraphData, true);

	if (GetDefault<UMetaRoadEditorSettings>()->bCallZoneGraphDataBuildDone)
	{
		bPreventGraphDataBuildDone = true;
		UE::ZoneGraphDelegates::OnZoneGraphDataBuildDone.Broadcast({});
		bPreventGraphDataBuildDone = false;
	}

	/*
	if (GCurrentLevelEditingViewportClient)
	{
		GCurrentLevelEditingViewportClient->Invalidate();
	}
	*/
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMetaRoadEditorModule, MetaRoadEditor)

