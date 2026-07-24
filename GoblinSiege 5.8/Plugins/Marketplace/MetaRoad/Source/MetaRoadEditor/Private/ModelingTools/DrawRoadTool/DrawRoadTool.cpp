/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/DrawRoadTool.h"
#include "Utils/DrawUtils.h"
#include "Utils/CompVisUtils.h"
#include "Utils/RoadUtils.h"
#include "LevelEditorViewport.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "AssetSelection.h" // FActorFactoryAssetProxy
#include "BaseBehaviors/SingleClickOrDragBehavior.h"
#include "BaseBehaviors/KeyInputBehavior.h"
#include "BaseGizmos/GizmoMath.h"
#include "RoadSplineComponent.h"
#include "CoreGlobals.h" // GUndo
#include "Drawing/PreviewGeometryActor.h"
#include "Editor/UnrealEdEngine.h" // DuplicateActors
#include "Engine/Blueprint.h"
#include "Engine/World.h" 
#include "GameFramework/Actor.h"
#include "ILevelEditor.h"
#include "InputState.h" // FInputDeviceRay
#include "InteractiveToolManager.h"
#include "ISceneOutliner.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Kismet2/ComponentEditorUtils.h" // GenerateValidVariableName
#include "Mechanics/ConstructionPlaneMechanic.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "Selection/ToolSelectionUtil.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SplineUtil.h"
#include "ToolBuilderUtil.h"
#include "UnrealEdGlobals.h" // GUnrealEd
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ModelingToolsEditorMode.h"
#include "InteractiveToolsContext.h"
#include "EngineUtils.h"
#include "MetaRoadEditorModule.h"
#include "MetaRoadSubsystem.h"
#include "MetaRoadModule.h"
#include "MetaRoadActor.h" // AMetaRoad (SpawnRoadActor return type)
#include "SPrimaryButton.h"
#include "Widgets/Images/SImage.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "SceneView.h"
#include "MetaRoadViewMatricesCompat.h"

#define LOCTEXT_NAMESPACE "UDrawRoadTool"

using namespace UE::Geometry;

struct HRoadLanePickerProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	HRoadLanePickerProxy(int InLaneIndex, EHitProxyPriority InPriority = HPP_UI)
		: HHitProxy(InPriority)
		, LaneIndex(InLaneIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}

	int LaneIndex;
};

IMPLEMENT_HIT_PROXY(HRoadLanePickerProxy, HHitProxy);


/**
 * Helper to hide actors from the outliner if it's not an actor that
 *  we defined to be automatically hidden (e.g. APreviewGeometryActor),
 *  that works by being friended to FSetActorHiddenInSceneOutliner.
 * This is a temporary measure until we have a cleaner way to hide
 *  ourselves from the outliner through TEDS.
 * Note that just creating this class doesn't actually refresh the outliner
 *  unless you happen to take an action that does (such as reparenting things),
 *  so you would need to call RefreshOutliner().
 */
class FModelingToolsSetActorHiddenInSceneOutliner
{
public:
	FModelingToolsSetActorHiddenInSceneOutliner(AActor* InActor, bool bHidden)
	{
		FSetActorHiddenInSceneOutliner Setter(InActor, bHidden);
	}

	/**
	 * Does a full refresh of the outliner. Note that this can be comparatively
	 *  slow, so it should happen rarely.
	 */
	void RefreshOutliner()
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (!LevelEditorModule)
		{
			return;
		}

		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetLevelEditorInstance().Pin();
		if (!LevelEditor.IsValid())
		{
			return;
		}

		for (TWeakPtr<ISceneOutliner> OutlinerWeak : LevelEditor->GetAllSceneOutliners())
		{
			if (TSharedPtr<ISceneOutliner> Outliner = OutlinerWeak.Pin())
			{
				Outliner->FullRefresh();
			}
		}
	}
};

namespace DrawSplineToolLocals
{
	FText AddPointTransactionName = LOCTEXT("AddPointTransactionName", "Add Point");




	ERoadLaneDirection ReverseDir(ERoadLaneDirection Dir)
	{
		return Dir == ERoadLaneDirection::Default ? ERoadLaneDirection::Invert : ERoadLaneDirection::Default;
	}

	// Gives the scale used for tangent visualization (and which therefore needs to be used in raycasting the handles)
	float GetTangentScale()
	{
		return GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;
	}

	// Might be useful to have in SplineUtil, but uncertain what the API should be (should it be part of
	// DrawSpline? Should there be options for selection color?). Also potentially messier to match the tangent
	// scale with the UI interaction..
	void DrawTangent(const URoadSplineComponent& SplineComp, int32 PointIndex, IToolsContextRenderAPI& RenderAPI)
	{
		if (!ensure(PointIndex >= 0 && PointIndex < SplineComp.GetNumberOfSplinePoints()))
		{
			return;
		}

		FPrimitiveDrawInterface* PDI = RenderAPI.GetPrimitiveDrawInterface();

		const float TangentScale = GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;
		const float TangentHandleSize = 8.0f + GetDefault<ULevelEditorViewportSettings>()->SplineTangentHandleSizeAdjustment;

		const FVector Location = SplineComp.GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
		const FVector LeaveTangent = SplineComp.GetLeaveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::World) * TangentScale;
		const FVector ArriveTangent = SplineComp.bAllowDiscontinuousSpline ? SplineComp.GetArriveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::World) * TangentScale : LeaveTangent;

		FColor Color = FColor::White;

		PDI->DrawLine(Location, Location - ArriveTangent, Color, SDPG_Foreground);
		PDI->DrawLine(Location, Location + LeaveTangent, Color, SDPG_Foreground);

		PDI->DrawPoint(Location + LeaveTangent, Color, TangentHandleSize, SDPG_Foreground);
		PDI->DrawPoint(Location - ArriveTangent, Color, TangentHandleSize, SDPG_Foreground);
	}

	// Helper class for making undo/redo transactions, to avoid friending all the variations.
	class FSplineChange : public FToolCommandChange
	{
	public:
		// These pass the working spline to the overloads below
		virtual void Apply(UObject* Object) override
		{
			UDrawRoadTool* Tool = Cast<UDrawRoadTool>(Object);
			if (!ensure(Tool))
			{
				return;
			}
			TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
			if (!ensure(Spline.IsValid()))
			{
				return;
			}

			Apply(*Spline);

			Tool->bNeedToRerunConstructionScript = true;
		}
		virtual void Revert(UObject* Object) override
		{
			UDrawRoadTool* Tool = Cast<UDrawRoadTool>(Object);
			if (!ensure(Tool))
			{
				return;
			}
			TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
			if (!ensure(Spline.IsValid()))
			{
				return;
			}

			Revert(*Spline);

			Tool->bNeedToRerunConstructionScript = true;
			Tool->EndLaneConnection = nullptr;

			if (Spline->GetNumberOfSplinePoints() == 0)
			{
				Tool->StartLaneConnection = nullptr;
				Tool->PickedUnderCursor = 0;
				Tool->PickedLeftNum = 0;
				Tool->PickedRightNum = 0;
			}

		}

	protected:
		virtual void Apply(URoadSplineComponent& Spline) = 0;
		virtual void Revert(URoadSplineComponent& Spline) = 0;
	};

	// Undoes a point addition with an auto tangent
	class FSimplePointInsertionChange : public FSplineChange
	{
	public:
		FSimplePointInsertionChange(const FVector3d& HitLocationIn, const FVector3d& UpVectorIn)
			: HitLocation(HitLocationIn)
			, UpVector(UpVectorIn)
		{
		}

		virtual void Apply(URoadSplineComponent& Spline) override
		{
			Spline.AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, false);
			int32 PointIndex = Spline.GetNumberOfSplinePoints() - 1;
			Spline.SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::World, true);
		}
		virtual void Revert(URoadSplineComponent& Spline) override
		{
			if (ensure(Spline.GetNumberOfSplinePoints() > 0))
			{
				Spline.RemoveSplinePoint(Spline.GetNumberOfSplinePoints() - 1, true);
			}
		}
		virtual FString ToString() const override
		{
			return TEXT("FSimplePointInsertionChange");
		}

	protected:
		FVector3d HitLocation;
		FVector3d UpVector;
	};

	// Undoes a point addition with an explicit tangent
	class FTangentPointInsertionChange : public FSplineChange
	{
	public:
		FTangentPointInsertionChange(const FVector3d& HitLocationIn, const FVector3d& UpVectorIn, const FVector3d& TangentIn)
			: HitLocation(HitLocationIn)
			, UpVector(UpVectorIn)
			, Tangent(TangentIn)
		{
		}

		virtual void Apply(URoadSplineComponent& Spline) override
		{
			Spline.AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, false);
			int32 PointIndex = Spline.GetNumberOfSplinePoints() - 1;
			Spline.SetUpVectorAtSplinePoint(PointIndex, UpVector, ESplineCoordinateSpace::World, false);
			Spline.SetTangentAtSplinePoint(PointIndex, Tangent, ESplineCoordinateSpace::World, true);
		}
		virtual void Revert(URoadSplineComponent& Spline) override
		{
			if (ensure(Spline.GetNumberOfSplinePoints() > 0))
			{
				Spline.RemoveSplinePoint(Spline.GetNumberOfSplinePoints() - 1, true);
			}
		}
		virtual FString ToString() const override
		{
			return TEXT("FTangentPointInsertionChange");
		}

	protected:
		FVector3d HitLocation;
		FVector3d UpVector;
		FVector3d Tangent;
	};

	FRoadLane MakeLaneFromTemplate(const FRoadLane& TemplateLane, bool bReverse, bool bFromEnd)
	{
		FRoadLane NewLane;

		NewLane.RoadZone = TemplateLane.RoadZone;

		const double SOffset = bFromEnd
			? TemplateLane.GetEndOffset() - TemplateLane.GetStartOffset() 
			: TemplateLane.GetStartOffset();

		const float BeginWidth = TemplateLane.Width.GetNumKeys()
			? TemplateLane.Width.Eval(SOffset)
			: MetaRoad::DefaultRoadLaneWidth;

		NewLane.Width.AddKey(0, BeginWidth);
		NewLane.Width.Keys[0].InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		NewLane.Width.Keys[0].TangentMode = ERichCurveTangentMode::RCTM_Auto;
		NewLane.Direction = bReverse ? ReverseDir(TemplateLane.Direction) : TemplateLane.Direction;

		// Copy omly first item of each attribute
		for (auto& [Name, Attribute] : TemplateLane.Attributes)
		{
			if (Attribute.Keys.Num() && IsValid(Attribute.GetScriptStruct()))
			{
				FRoadLaneAttribute NewAttribute;
				NewAttribute.SetScriptStruct(const_cast<UScriptStruct*>(Attribute.GetScriptStruct()));
				const int Index = Attribute.FindKeyBeforeOrAt(SOffset);
				check(Index >= 0);
				NewAttribute.Keys.Add(Attribute.Keys[Index]);
				NewLane.Attributes.Add(Name, NewAttribute);
			}
		}

		return NewLane;
	}

	void FitSplineToBeginConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* LaneConnection, bool bUpdateSpline)
	{
		check(TargetSpline);
		check(TargetSpline->GetNumberOfSplinePoints() >= 2);

		FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
		double TangentSize = (TargetSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World) - Transform.GetLocation()).Size2D();
		FVector ForwardVector = Transform.GetRotation().GetForwardVector();
		TargetSpline->SetTangentAtSplinePoint(0, ForwardVector * TangentSize, ESplineCoordinateSpace::World, bUpdateSpline);
		
	}

	void FitSplineToEndConnection(URoadSplineComponent* TargetSpline, const ULaneConnection* LaneConnection, bool bUpdateSpline)
	{
		check(TargetSpline);
		check(LaneConnection)
		check(TargetSpline->GetNumberOfSplinePoints() >= 2);

		int LastPointIndex = TargetSpline->GetNumberOfSplinePoints() - 1;
		const FTransform Transform = LaneConnection->EvalTransform(0.0, ESplineCoordinateSpace::World);
		const double TangentSize = (TargetSpline->GetLocationAtSplinePoint(LastPointIndex - 1, ESplineCoordinateSpace::World) - Transform.GetLocation()).Size2D();
		TargetSpline->SetRotationAtSplinePoint_Fixed(LastPointIndex, Transform.Rotator(), ESplineCoordinateSpace::World, false);
		auto& Point = TargetSpline->SplineCurves.Position.Points[LastPointIndex];
		Point.LeaveTangent *= TangentSize;
		Point.ArriveTangent *= TangentSize;
		Point.InterpMode = EInterpCurveMode::CIM_CurveUser;

		if (bUpdateSpline)
		{
			TargetSpline->UpdateSpline();
		}
	}


	template <typename Predicate>
	FName CreateUniqueName(const FName& InBaseName, Predicate IsUnique)
	{
		FName CurrentName = InBaseName;
		int32 CurrentIndex = 0;

		while (!IsUnique(CurrentName))
		{
			FString PossibleName = InBaseName.ToString() + TEXT("_") + FString::FromInt(CurrentIndex++);
			CurrentName = FName(*PossibleName);
		}

		return CurrentName;
	}

	UEditorInteractiveToolsContext* GetInteractiveToolsContext()
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
		{
			if (TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin())
			{
				FEditorModeTools& EditorModeTools = LevelEditorPtr->GetEditorModeManager();
				if (UEdMode* EdMode = EditorModeTools.GetActiveScriptableMode(UModelingToolsEditorMode::EM_ModelingToolsEditorModeId))
				{
					return EdMode->GetInteractiveToolsContext(EToolsContextScope::EdMode);
				}
			}
		}
		return nullptr;
	}

	void DrawLanePicker(ULaneConnection* Connection, const FColor& Color, class FPrimitiveDrawInterface* PDI, const FSceneView* View, uint8 DepthPriorityGroup)
	{
		const int32 NumSides = 10;
		const double Radius = 3;
		const double Offset = 15;

		const auto Transform = Connection->EvalTransform(0.5, ESplineCoordinateSpace::World);
		float ViewScale = static_cast<float>(View->WorldToScreen(Transform.GetLocation()).W * (4.0f / View->UnscaledViewRect.Width() / MetaRoad::ViewCompat::GetViewToClip(View->ViewMatrices).M[0][0]));
		const auto Quat = Transform.GetRotation();
		const auto Center = Transform.GetLocation() + Quat.GetForwardVector() * Offset * (Connection->IsSuccessorConnection() ? +1 : -1) * ViewScale;

		
		//ViewScale = FMath::Clamp(ViewScale, 5.0, 10.0);

		auto& Mat = GetDefault<UMetaRoadEditorSettings>()->UIVertexColorMaterial;
		check(Mat);

		const FMatrix& ViewMatrix = MetaRoad::ViewCompat::GetWorldToView(PDI->View->ViewMatrices);
		const FVector XAxis = ViewMatrix.GetColumn(0);
		const FVector YAxis = ViewMatrix.GetColumn(1);

		DrawUtils::DrawDisc(PDI, Center, XAxis, YAxis, Color, Radius * ViewScale, NumSides, Mat->GetRenderProxy(), DepthPriorityGroup);
	}

}

// -------------------------------------------------------------------------------------------------------------------
UDrawRoadToolProperties::UDrawRoadToolProperties()
{
	DrawProfile = TSoftObjectPtr<URoadProfile>(FSoftObjectPath(TEXT("/MetaRoad/MetaRoad/Profiles/RoadProfiles/2_Lanes+Borders.2_Lanes+Borders"))).LoadSynchronous();
}

bool UDrawRoadToolProperties::DrawProfileIsValid() const
{
	return DrawRoadTool.IsValid() && !DrawRoadTool->StartLaneConnection.IsValid();
}

// -------------------------------------------------------------------------------------------------------------------
void UDrawRoadTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<UDrawRoadToolProperties>(this);
	Settings->RestoreProperties(this);
	Settings->DrawRoadTool = this;
	AddToolPropertySource(Settings);

	SetToolDisplayName(LOCTEXT("DrawSplineToolName", "Draw Spline"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("DrawSplineToolDescription", "Draw a spline to replace an existing one or add it to an actor."),
		EToolMessageLevel::UserNotification);

	PlaneMechanic = NewObject<UConstructionPlaneMechanic>(this);
	PlaneMechanic->Setup(this);
	PlaneMechanic->Initialize(GetTargetWorld(), FFrame3d(FVector3d::Zero(), FVector3d::UnitX()));
	PlaneMechanic->bShowGrid = Settings->bHitCustomPlane;
	PlaneMechanic->CanUpdatePlaneFunc = [this] { return Settings->bHitCustomPlane; };
	Settings->WatchProperty(Settings->bHitCustomPlane, [this](bool) 
	{
		PlaneMechanic->bShowGrid = Settings->bHitCustomPlane;
	});

	ClickOrDragBehavior = NewObject<USingleClickOrDragInputBehavior>();
	ClickOrDragBehavior->Initialize(this, this);
	AddInputBehavior(ClickOrDragBehavior);

	//KeyInputBehavior = NewObject<UKeyInputBehavior>();
	//KeyInputBehavior->Initialize(this, { EKeys::LeftControl });
	//AddInputBehavior(KeyInputBehavior);

	// Make sure the plane mechanic captures clicks first, to ensure it sees ctrl+clicks to reposition the plane
	PlaneMechanic->UpdateClickPriority(ClickOrDragBehavior->GetPriority().MakeHigher());

	Settings->WatchProperty(Settings->bLoop, [this](bool)
	{
		if (ensure(WorkingSpline.IsValid()))
		{
			WorkingSpline->SetClosedLoop(Settings->bLoop);
			bNeedToRerunConstructionScript = true;
		}
	});

	ReCreatePreview();

	Settings->WatchProperty(Settings->LoopedRoadZone, [this](const TInstancedStruct<FRoadZone>& LoopedRoadZone)
	{
		ReCreatePreview();
	});

	Settings->WatchProperty(Settings->bCreateBlueprint, [this](bool)
	{
		ReCreatePreview();
	});

	//Settings->WatchProperty(Settings->bUseProfile, [this](bool)
	//{
	//	ReCreatePreview();
	//});

	Settings->WatchProperty(Settings->BlueprintToCreate, [this](TWeakObjectPtr<UBlueprint>) 
	{
		ReCreatePreview();
	});

	Settings->SilentUpdateWatched();

	CapturedConnections.Empty();
}

// Set things up for a new output mode or destination
void UDrawRoadTool::ReCreatePreview()
{
	using namespace DrawSplineToolLocals;

	// Setting up the previews seems to be the most error prone part of the tool because editor duplicating, hiding
	// from outliner, and avoiding emitting undo/redo transactions seems to be quite finnicky...

	// This function is sometimes called from inside transactions (such as tool start, or dragging the "component to replace"
	// slider). Several calls here would transact in that case (for instance, the Destroy() calls on the previews seem
	// to do it), which we generally don't want to do. So we disable transacting in this function with the hack below.
	// Note that we still have to take care that any editor functions we call don't open their own transactions...
	ITransaction* UndoState = GUndo;
	GUndo = nullptr; // Pretend we're not in a transaction
	ON_SCOPE_EXIT{ GUndo = UndoState; }; // Revert later


	// Keep the previous spline/preview temporarily so we can transfer over spline data
	// when we make new previews
	URoadSplineComponent* PreviousSpline = WorkingSpline.Get();

	if (WorkingSpline.IsValid())
	{
		WorkingSpline->DestroyComponent();
	}

	if (PreviewActor)
	{
		PreviewActor->Destroy();
	}

	PreviewActor = nullptr;
	WorkingSpline = nullptr;
	
	auto FallbackSplinePlacement = [this]()
	{
		FRotator Rotation(0.0f, 0.0f, 0.0f);
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags = RF_Transient;
		PreviewActor = GetTargetWorld()->SpawnActor<APreviewGeometryActor>(FVector::ZeroVector, Rotation, SpawnInfo);
		USceneComponent* RootComponent = NewObject<USceneComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(RootComponent);
		PreviewActor->SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();
		WorkingSpline = RoadUtils::CreateSplineInActor(PreviewActor);
		WorkingSpline->GetRoadLayout().LoopedRoadZone = Settings->LoopedRoadZone;
	};

	// Set up the new preview
	if (SelectedActor.IsValid() || !Settings->bCreateBlueprint)
	{
		FallbackSplinePlacement();
	}
	else if( Settings->bCreateBlueprint)
	{
		if (Settings->BlueprintToCreate.IsValid()
			&& Settings->BlueprintToCreate->GeneratedClass != nullptr
			&& !Settings->BlueprintToCreate->GeneratedClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract))
		{
			// Instantiate the blueprint
			PreviewActor = FActorFactoryAssetProxy::AddActorForAsset(
				Settings->BlueprintToCreate.Get(),
				/*bSelectActors =*/ false,
				// Important that we don't use the default (RF_Transactional) here, or else we'll end up
				// issuing an undo transaction in this call.
				EObjectFlags::RF_Transient);

			if (PreviewActor)
			{
				// Hide this preview from outliner
				FModelingToolsSetActorHiddenInSceneOutliner Hider(PreviewActor, true);
				Hider.RefreshOutliner();

				WorkingSpline = RoadUtils::CreateSplineInActor(PreviewActor);
				bNeedToRerunConstructionScript = true;
			}
			else
			{
				FallbackSplinePlacement();
			}
		}
		else
		{
			FallbackSplinePlacement();
		}
	}

	if (ensure(WorkingSpline.IsValid()))
	{
		if (PreviousSpline)
		{
			RoadUtils::CopySplineToSpline(*PreviousSpline, *WorkingSpline);
		}
		else
		{
			WorkingSpline->ClearSplinePoints();
		}

		InitRoadProfile(WorkingSpline.Get());

		WorkingSpline->SetClosedLoop(Settings->bLoop);

		// This has to be set so that construction script reruns transfer over current spline state.
		WorkingSpline->bSplineHasBeenEdited = true;

		// Get the index of the spline in the components array for recapturing on construction script reruns.
		if (PreviewActor)
		{
			TInlineComponentArray<URoadSplineComponent*> SplineComponents;
			PreviewActor->GetComponents<URoadSplineComponent>(SplineComponents);
			SplineRecaptureIndex = SplineComponents.IndexOfByKey(WorkingSpline.Get());
			ensure(SplineRecaptureIndex >= 0);
		}
	}
}

void UDrawRoadTool::Shutdown(EToolShutdownType ShutdownType)
{
	using namespace DrawSplineToolLocals;

	LongTransactions.CloseAll(GetToolManager());

	Settings->SaveProperties(this);

	if (ShutdownType == EToolShutdownType::Accept && WorkingSpline.IsValid() && WorkingSpline->GetNumberOfSplinePoints() > 1)
	{
		GenerateAsset();
	}

	PlaneMechanic->Shutdown();
	
	
	if (WorkingSpline.IsValid())
	{
		WorkingSpline->DestroyComponent();
	}

	if (PreviewActor)
	{
		PreviewActor->Destroy();
	}

	CapturedConnections.Empty();

	Super::Shutdown(ShutdownType);
}

void UDrawRoadTool::GenerateAsset()
{
	using namespace DrawSplineToolLocals;

	URoadSplineComponent* OutputSpline = nullptr;

	auto CreateSpline = [this](AActor* TargetActor, bool bRerunConstructionScripts)
	{
		TargetActor->Modify();

		FName NewComponentName = *FComponentEditorUtils::GenerateValidVariableName(URoadSplineComponent::StaticClass(), TargetActor);
		URoadSplineComponent* OutputSpline = NewObject<URoadSplineComponent>(TargetActor, URoadSplineComponent::StaticClass(), NewComponentName, RF_Transactional);
		OutputSpline->SetupAttachment(TargetActor->GetRootComponent());
		OutputSpline->OnComponentCreated();
		TargetActor->AddInstanceComponent(OutputSpline);
		OutputSpline->ResetRelativeTransform();
		OutputSpline->RegisterComponent();

		OutputSpline->GetRoadLayout() = WorkingSpline->GetRoadLayout();
		OutputSpline->GetRoadLayout().UpdateLayout(OutputSpline);
		RoadUtils::CopySplineToSpline(*WorkingSpline, *OutputSpline, true);

		if (StartLaneConnection.IsValid())
		{
			OutputSpline->GetPredecessorConnection()->ConnectTo(StartLaneConnection.Get());
			OutputSpline->GetPredecessorConnection()->SetTransformFormOuter();
		}

		if (EndLaneConnection.IsValid())
		{
			OutputSpline->GetSuccessorConnection()->ConnectTo(EndLaneConnection.Get());
			FitSplineToEndConnection(OutputSpline, EndLaneConnection.Get(), true);
			OutputSpline->GetSuccessorConnection()->SetTransformFormOuter();
			RoadUtils::FitLanesWidthToEndConnection(OutputSpline, EndLaneConnection.Get());
		}

		OutputSpline->PostEditChange();

		if (bRerunConstructionScripts)
		{
			TargetActor->RerunConstructionScripts();
		}
		return OutputSpline;
	};

	auto CreateSplineAndActor = [this, &CreateSpline]()
	{
		// Get centroid of spline
		int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
		FVector3d Center = FVector3d::Zero();
		for (int32 i = 0; i < NumSplinePoints; ++i)
		{
			Center += WorkingSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
		}
		Center /= NumSplinePoints;

		AActor* NewActor = RoadUtils::SpawnRoadActor(TargetWorld.Get(), FTransform(Center));


		return CreateSpline(NewActor, false);
	};

	GetToolManager()->BeginUndoTransaction(LOCTEXT("DrawSplineTransactionName", "Draw Spline"));


	if (bShutdownReqAddToSelectedActor)
	{
		if (SelectedActor.IsValid())
		{
			CreateSpline(SelectedActor.Get(), false);
		}
	}
	else
	{
		if (Settings->bCreateBlueprint)
		{
			bool bCanCreateActor = Settings->BlueprintToCreate.IsValid()// != nullptr
				&& Settings->BlueprintToCreate->GeneratedClass != nullptr
				&& !Settings->BlueprintToCreate->GeneratedClass->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Abstract);

			if (bCanCreateActor)
			{
				if (AActor* NewActor = FActorFactoryAssetProxy::AddActorForAsset(Settings->BlueprintToCreate.Get(), /*bSelectActors =*/ false))
				{
					OutputSpline = CreateSpline(NewActor, true);
				}
			}
		}
		else
		{
			OutputSpline = CreateSplineAndActor();
		}
	}


	// TODO: Someday when we support component selection, we should select OutputSpline directly.
	if (OutputSpline)
	{
		ToolSelectionUtil::SetNewActorSelection(GetToolManager(), OutputSpline->GetAttachmentRootActor());
	}

	GetToolManager()->EndUndoTransaction();
}

// Helper to add a point given a hit location and hit normal
void UDrawRoadTool::AddSplinePoint(const FVector3d& HitLocation, const FVector3d& HitNormal, bool bUpdateSpline)
{
	using namespace DrawSplineToolLocals;
	if (!WorkingSpline.IsValid())
	{
		return;
	}

	int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
	FVector3d UpVectorToUse = GetUpVectorToUse(HitLocation, HitNormal, NumSplinePoints);

	WorkingSpline->AddSplinePoint(HitLocation, ESplineCoordinateSpace::World, false);
	//WorkingSpline->SetUpVectorAtSplinePoint(NumSplinePoints, UpVectorToUse, ESplineCoordinateSpace::World, false);

	if (bUpdateSpline)
	{
		WorkingSpline->UpdateSpline();
	}
}

FVector3d UDrawRoadTool::GetUpVectorToUse(const FVector3d& HitLocation, const FVector3d& HitNormal, int32 NumSplinePointsBeforehand)
{
	FVector3d UpVectorToUse = HitNormal;
	switch (Settings->UpVectorMode)
	{
	case EDrawRoadUpVectorMode::AlignToPrevious:
	{
		if (NumSplinePointsBeforehand == 0)
		{
			// TODO: Maybe add some different options of what normal to start with
		}
		else if (NumSplinePointsBeforehand > 1)
		{
			UpVectorToUse = WorkingSpline->GetUpVectorAtSplinePoint(NumSplinePointsBeforehand - 1, ESplineCoordinateSpace::World);
		}
		else // if NumSplinePointsBeforehand == 1
		{
			// If there's only one point, GetUpVectorAtSplinePoint is unreliable because it seeks to build a
			// quaternion from the tangent and the set up vector, and the tangent is zero. We want to use
			// the "stored" up vector directly.
			FVector3d LocalUpVector = WorkingSpline->SplineCurves.Rotation.Points[0].OutVal.RotateVector(WorkingSpline->DefaultUpVector);
			UpVectorToUse = WorkingSpline->GetComponentTransform().TransformVectorNoScale(LocalUpVector);
		}
		break;
	}
	case EDrawRoadUpVectorMode::UseHitNormal:
		break;
	}

	return UpVectorToUse;
}

bool UDrawRoadTool::Raycast(const FRay& InWorldRay, FVector3d& HitLocationOut, FVector3d& HitNormalOut, double& HitTOut) const
{
	double BestHitT = TNumericLimits<double>::Max();

	FRay WorldRay = InWorldRay;
	if (CameraState.bIsOrthographic)
	{
		// Fix buf bug of Orthographic projection in UE 5.5. 
		// Deatils: for Orthographic projection WorldRay.Origin.Z > max float32 precision, but type of FHitResult.Distance is float32.
		// Therefore the distance to the nearest object is not determined correctly
		WorldRay.Origin.Z = 4'194'304;
	}
	
	if (Settings->bHitCustomPlane)
	{
		FVector IntersectionPoint;
		bool bHitPlane = false;
		GizmoMath::RayPlaneIntersectionPoint(PlaneMechanic->Plane.Origin, PlaneMechanic->Plane.Z(),
			WorldRay.Origin, WorldRay.Direction, bHitPlane, IntersectionPoint);

		if (bHitPlane)
		{
			HitLocationOut = IntersectionPoint;
			HitNormalOut = PlaneMechanic->Plane.Z();
			HitTOut = WorldRay.GetParameter(IntersectionPoint);
			BestHitT = HitTOut;
		}
	}

	if (Settings->bHitWorld)
	{
		FHitResult GeometryHit;
		TArray<const UPrimitiveComponent*> ComponentsToIgnore;
		if (PreviewActor)
		{
			PreviewActor->GetComponents<const UPrimitiveComponent>(ComponentsToIgnore);
		}
		if (ToolSceneQueriesUtil::FindNearestVisibleObjectHit(this, GeometryHit, WorldRay, &ComponentsToIgnore)
			&& GeometryHit.Distance < BestHitT)
		{
			HitLocationOut = GeometryHit.ImpactPoint;
			HitNormalOut = GeometryHit.ImpactNormal;
			HitTOut = GeometryHit.Distance;
			BestHitT = HitTOut;
		}
	}

	// Only raycast the ground plane / ortho background if we didn't hit anything else
	if (Settings->bHitGroundPlanes && BestHitT == TNumericLimits<double>::Max())
	{
		FVector3d PlaneNormal = CameraState.bIsOrthographic ? -WorldRay.Direction : FVector3d::UnitZ();
		FVector IntersectionPoint;
		bool bHitPlane = false;
		GizmoMath::RayPlaneIntersectionPoint(FVector3d::Zero(), PlaneNormal,
			WorldRay.Origin, WorldRay.Direction, bHitPlane, IntersectionPoint);

		if (bHitPlane)
		{
			HitLocationOut = IntersectionPoint;
			HitNormalOut = PlaneNormal;
			HitTOut = WorldRay.GetParameter(IntersectionPoint);
			BestHitT = HitTOut;
		}
	}

	if (Settings->ClickOffset != 0.0)
	{
		FVector3d OffsetDirection = HitNormalOut;
		HitLocationOut += OffsetDirection * Settings->ClickOffset;
	}
	
	return BestHitT < TNumericLimits<double>::Max();
}

bool UDrawRoadTool::MouseTrace(const FRay& WorldRay, FMouseTraceResult& Result) const
{
	if (ConnectionUnderCursor.IsValid())
	{
		FTransform Transform = ConnectionUnderCursor->EvalTransform(0.0, ESplineCoordinateSpace::World);
		Result.Location = Transform.GetLocation();
		Result.UpVector = Transform.GetRotation().GetUpVector();
		Result.ForwardVector = Transform.GetRotation().GetForwardVector();
		Result.Connection = ConnectionUnderCursor;
		Result.HitT = (Transform.GetLocation() - WorldRay.Origin).Size();
		return true;
	}

	if (Raycast(WorldRay, Result.Location, Result.UpVector, Result.HitT))
	{
		Result.ForwardVector = FVector3d::Zero();
		return true;
	}

	return false;
}

/*
bool UDrawRoadTool::FinishDraw()
{
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		if (TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin())
		{
			FEditorModeTools& EditorModeTools = LevelEditorPtr->GetEditorModeManager();
			if (UEdMode* EdMode = EditorModeTools.GetActiveScriptableMode(UMetaRoadEditorMode::EM_MetaRoadEditorModeId))
			{
				EdMode->GetToolManager()->DeactivateTool(EToolSide::Left, EToolShutdownType::Accept);
				return true;
			}
		}
	}

	return false;
}
*/

FInputRayHit UDrawRoadTool::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	/*
	FMouseTraceResult HitResult;
	if (MouseTrace(ClickPos.WorldRay, HitResult))
	{
		return FInputRayHit(HitResult.HitT);
	}
	*/
	return FInputRayHit { true };
}

void UDrawRoadTool::SetStartConnection(ULaneConnection* Connection)
{
	if (IsValid(Connection))
	{
		PickedRightNum = 0;
		PickedLeftNum = 0;
		StartLaneConnection = Connection;

		auto& BaseLane = StartLaneConnection->GetOwnedRoadLane();
		const int BaseLaneIndex = BaseLane.GetLaneIndex();
		if (BaseLaneIndex > 0)
		{
			PickedRightNum = 1;
		}
		else
		{
			PickedLeftNum = 1;
		}
	}
}

void UDrawRoadTool::InitRoadProfile(URoadSplineComponent* TargetSpline) const
{
	using namespace DrawSplineToolLocals;

	if (StartLaneConnection.IsValid())
	{
		TargetSpline->GetLaneSections().Empty();
		auto& OutSection = TargetSpline->GetLaneSections().Add_GetRef({});
		auto& BaseLayout = StartLaneConnection->GetOwnedRoadSpline()->GetRoadLayout();
		auto& BaseLane = StartLaneConnection->GetOwnedRoadLane();
		const auto& [LeftLanes, RightLanes] = BaseLayout.GetLeftRighLanes(BaseLane.IsForwardLane() ? BaseLane.GetEndSectionIndex() : BaseLane.GetStartSectionIndex());
		const int BaseLaneIndex = BaseLane.GetLaneIndex();

		TargetSpline->GetRoadLayout().Direction = BaseLayout.Direction;

		auto GetLaneByIndex = [&LeftLanes, &RightLanes](int LaneIndex)
		{
			check(LaneIndex != 0);
			return LaneIndex > 0 ? RightLanes[LaneIndex - 1] : LeftLanes[-LaneIndex - 1];
		};

		auto CheckLaneIndex = [&LeftLanes, &RightLanes](int LaneIndex)
		{
			if(LaneIndex == 0)
			{
				return false;
			}
			if (LaneIndex > 0 && LaneIndex > RightLanes.Num())
			{
				return false;
			}
			if (LaneIndex < 0 && -LaneIndex > LeftLanes.Num())
			{
				return false;
			}
			return true;
		};

		if (BaseLane.IsForwardLane())
		{
			for (int i = 0; i < PickedRightNum; ++i)
			{
				const int LaneIndex = RoadUtils::GetRightOf(BaseLaneIndex, i);
				if (ensure(CheckLaneIndex(LaneIndex)))
				{
					OutSection.Right.Add(MakeLaneFromTemplate(GetLaneByIndex(LaneIndex), LaneIndex < 0, true));
				}
			}

			for (int i = 0; i < PickedLeftNum; ++i)
			{
				const int LaneIndex = RoadUtils::GetLeftOf(BaseLaneIndex, i);
				if (ensure(CheckLaneIndex(LaneIndex)))
				{
					OutSection.Left.Add(MakeLaneFromTemplate(GetLaneByIndex(LaneIndex), LaneIndex > 0, true));
				}
			}
		}
		else
		{
			for (int i = 0; i < PickedRightNum; ++i)
			{
				const int LaneIndex = RoadUtils::GetRightOf(BaseLaneIndex, i);
				if (ensure(CheckLaneIndex(LaneIndex)))
				{
					OutSection.Left.Add(MakeLaneFromTemplate(GetLaneByIndex(LaneIndex), LaneIndex < 0, false));
				}
			}

			for (int i = 0; i < PickedLeftNum; ++i)
			{
				const int LaneIndex = RoadUtils::GetLeftOf(BaseLaneIndex, i);
				if (ensure(CheckLaneIndex(LaneIndex)))
				{
					OutSection.Right.Add(MakeLaneFromTemplate(GetLaneByIndex(LaneIndex), LaneIndex > 0, false));
				}
			}
		}

		TargetSpline->UpdateRoadLayout();
	}
	else
	{
		if (IsValid(Settings->DrawProfile))
		{
			Settings->DrawProfile->AssignToRoadSpline(TargetSpline);
		}
		else
		{
			TargetSpline->GetLaneSections().Empty();
			TargetSpline->GetLaneSections().AddDefaulted();
			TargetSpline->UpdateRoadLayout();
		}
	}
	
	TargetSpline->GetRoadLayout().LoopedRoadZone = Settings->LoopedRoadZone;
	TargetSpline->MarkRenderStateDirty();
}


void UDrawRoadTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	using namespace DrawSplineToolLocals;

	if (!WorkingSpline.IsValid())
	{
		return;
	}

	if (PickedUnderCursor != 0 && StartLaneConnection.IsValid())
	{
		auto& BaseLane = StartLaneConnection->GetOwnedRoadLane();
		auto& BaseLayout = StartLaneConnection->GetOwnedRoadSpline()->GetRoadLayout();
		const auto& [LeftLanes, RightLanes] = BaseLayout.GetLeftRighLanes(BaseLane.IsForwardLane() ? BaseLane.GetEndSectionIndex() : BaseLane.GetStartSectionIndex());
		const int BaseLaneIndex = BaseLane.GetLaneIndex();
		const int BaseLaneIndexSeq = MetaRoad::LaneIndex2SeqIndex(LeftLanes.Num(), BaseLaneIndex);
		const int CursorLaneIndexSeq = MetaRoad::LaneIndex2SeqIndex(LeftLanes.Num(), PickedUnderCursor);
		const int Shift = BaseLaneIndexSeq - CursorLaneIndexSeq;

		auto SetPickedLeftNum = [this](int Num) 
		{
			PickedLeftNum = FMath::Max(PickedLeftNum == Num ? PickedLeftNum - 1 : Num, 0);
		};
		auto SetPickedRightNum = [this](int Num)
		{
			PickedRightNum = FMath::Max(PickedRightNum == Num ? PickedRightNum - 1 : Num, 0);
		};

		if (BaseLaneIndex > 0)
		{
			if (Shift > 0)
			{
				SetPickedLeftNum(Shift);
			}
			else
			{
				SetPickedRightNum(-Shift + 1);
			}
		}
		else
		{
			if (Shift >= 0)
			{
				SetPickedLeftNum(Shift + 1);
			}
			else
			{
				SetPickedRightNum(-Shift);
			}
		}
		InitRoadProfile(WorkingSpline.Get());
		RoadUtils::FitLanesWidthToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get());
		return;
	}

	if (EndLaneConnection.IsValid())
	{
		return;
	}

	FMouseTraceResult HitResult;
	if (!MouseTrace(ClickPos.WorldRay, HitResult))
	{
		return;
	}

	AddSplinePoint(HitResult.Location, HitResult.UpVector, false);

	const int NumPoints = WorkingSpline->GetNumberOfSplinePoints();
	const int32 PointIndex = NumPoints - 1;

	if (NumPoints == 1)
	{
		if (HitResult.Connection.IsValid())
		{
			SetStartConnection(HitResult.Connection.Get());
		}

		InitRoadProfile(WorkingSpline.Get());
		bConnectionsCashIsDirty = true;
	}
	else if (NumPoints == 2 && StartLaneConnection.IsValid())
	{
		FitSplineToBeginConnection(WorkingSpline.Get(), StartLaneConnection.Get(), false);
	}

	if (HitResult.Connection.IsValid() && NumPoints > 1)
	{
		EndLaneConnection = HitResult.Connection.Get();
		FitSplineToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get(), false);
	}

	if (NumPoints > 1)
	{
		WorkingSpline->UpdateSpline();
	}

	RoadUtils::FitLanesWidthToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get());

	GetToolManager()->EmitObjectChange(this,
		MakeUnique<FTangentPointInsertionChange>(
			HitResult.Location,
			WorkingSpline->GetUpVectorAtSplinePoint(PointIndex, ESplineCoordinateSpace::World),
			HitResult.ForwardVector),
		AddPointTransactionName);

	bNeedToRerunConstructionScript = true;
}

FInputRayHit UDrawRoadTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FMouseTraceResult HitResult;
	if (MouseTrace(PressPos.WorldRay, HitResult))
	{
		return FInputRayHit(HitResult.HitT);
	}
	return FInputRayHit();
}

void UDrawRoadTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	using namespace DrawSplineToolLocals;

	if (EndLaneConnection.IsValid())
	{
		return;
	}

	FMouseTraceResult HitResult;

	LongTransactions.Open(DrawSplineToolLocals::AddPointTransactionName, GetToolManager());

	// Regardless of DrawMode, start by placing a point, though don't emit a transaction until mouse up
	if (ensure(MouseTrace(PressPos.WorldRay, HitResult)))
	{
		AddSplinePoint(HitResult.Location, HitResult.UpVector, true);

		bNeedToRerunConstructionScript = bNeedToRerunConstructionScript || Settings->bRerunConstructionScriptOnDrag;
	}
}

void UDrawRoadTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	using namespace DrawSplineToolLocals;

	if (EndLaneConnection.IsValid())
	{
		return;
	}

	int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();
	if (!ensure(NumSplinePoints > 0))
	{
		return;
	}

	FMouseTraceResult HitResult;
	if (!MouseTrace(DragPos.WorldRay, HitResult))
	{
		return;
	}
	
	const int LastPointIndex = NumSplinePoints - 1;

	// Drag the last placed point
	WorkingSpline->SetLocationAtSplinePoint(LastPointIndex, HitResult.Location, ESplineCoordinateSpace::World, false);
	auto& Point = WorkingSpline->SplineCurves.Position.Points[LastPointIndex];

	if (NumSplinePoints == 2 && StartLaneConnection.IsValid())
	{
		FitSplineToBeginConnection(WorkingSpline.Get(), StartLaneConnection.Get(), false);
	}

	if (HitResult.Connection.IsValid() && NumSplinePoints > 1)
	{
		FitSplineToEndConnection(WorkingSpline.Get(), HitResult.Connection.Get(), false);
	}

	WorkingSpline->UpdateSpline();

	RoadUtils::FitLanesWidthToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get());
	
	bNeedToRerunConstructionScript = bNeedToRerunConstructionScript || Settings->bRerunConstructionScriptOnDrag;
}

void UDrawRoadTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	using namespace DrawSplineToolLocals;

	if (EndLaneConnection.IsValid())
	{
		return;
	}

	OnClickDrag(ReleasePos);

	FMouseTraceResult HitResult;
	if (!MouseTrace(ReleasePos.WorldRay, HitResult))
	{
		return;
	}

	if (HitResult.Connection.IsValid())
	{
		if (WorkingSpline->GetNumberOfSplinePoints() == 1)
		{
			SetStartConnection(HitResult.Connection.Get());
		}

		if (WorkingSpline->GetNumberOfSplinePoints() > 1)
		{
			EndLaneConnection = HitResult.Connection;
			FitSplineToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get(), true);
			RoadUtils::FitLanesWidthToEndConnection(WorkingSpline.Get(), EndLaneConnection.Get());
		}
	}

	if (WorkingSpline->GetNumberOfSplinePoints() == 1)
	{
		InitRoadProfile(WorkingSpline.Get());
		bConnectionsCashIsDirty = true;
	}

	OnTerminateDragSequence();
}

void UDrawRoadTool::OnTerminateDragSequence()
{
	using namespace DrawSplineToolLocals;

	bDrawTangentForLastPoint = false;
	bNeedToRerunConstructionScript = true;

	const int32 NumSplinePoints = WorkingSpline->GetNumberOfSplinePoints();

	// Emit the appropriate undo transaction
	GetToolManager()->EmitObjectChange(this,
		MakeUnique<FSimplePointInsertionChange>(
			WorkingSpline->GetLocationAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World),
			WorkingSpline->GetUpVectorAtSplinePoint(NumSplinePoints - 1, ESplineCoordinateSpace::World)),
		AddPointTransactionName);


	LongTransactions.Close(GetToolManager());
}

void UDrawRoadTool::OnTick(float DeltaTime)
{
	if (PlaneMechanic)
	{
		PlaneMechanic->Tick(DeltaTime);
	}

	// check if we've invalidated the WorkingSpline
	if (PreviewActor && !WorkingSpline.IsValid())
	{
		bNeedToRerunConstructionScript = true;
	}

	if (bNeedToRerunConstructionScript)
	{ 
		bNeedToRerunConstructionScript = false;
		if (PreviewActor)
		{
			PreviewActor->RerunConstructionScripts();

			// Rerunning the construction script can make us lose our reference to the spline, so try to
			// recapture.
			// TODO: This might be avoidable with FComponentReference?
			if (!WorkingSpline.IsValid())
			{
				TInlineComponentArray<URoadSplineComponent*> SplineComponents;
				PreviewActor->GetComponents<URoadSplineComponent>(SplineComponents);

				if (ensure(SplineRecaptureIndex >= 0 && SplineRecaptureIndex < SplineComponents.Num()))
				{
					WorkingSpline = SplineComponents[SplineRecaptureIndex];
				}
			}
		}
	}

	if (!WorkingSpline.IsValid())
	{
		GetToolManager()->PostActiveToolShutdownRequest(this, EToolShutdownType::Cancel, true, LOCTEXT("LostWorkingSpline", "The Draw Spline tool must close because the in-progress spline has been unexpectedly deleted."));
	}

	if (bConnectionsCashIsDirty)
	{
		CapturedConnections = RoadUtils::CaptureConnections(
			WorkingSpline->GetNumberOfSplinePoints() == 0 ? WorkingSpline->GetPredecessorConnection() : WorkingSpline->GetSuccessorConnection(),
			RoadUtils::FViewCameraState{
				CashedViewToProj,
				CashedViewRect,
				CameraState.Position,
				CameraState.bIsOrthographic,
				CameraState.OrthoWorldCoordinateWidth },
			GetDefault<UMetaRoadEditorSettings>()->RoadConnectionsMaxViewDistance,
			GetDefault<UMetaRoadEditorSettings>()->RoadConnectionMaxViewOrthoWidth);
		bConnectionsCashIsDirty = false;
	}

	ConnectionUnderCursor = nullptr;
	PickedUnderCursor = 0;

	if (IToolsContextQueriesAPI* ContextAPI = GetToolManager()->GetContextQueriesAPI())
	{
		if (FViewport* Viewport = ContextAPI->GetFocusedViewport())
		{
			HHitProxy* HitProx = Viewport->GetHitProxy(Viewport->GetMouseX(), Viewport->GetMouseY());

			if (auto* RoadLaneProxy = HitProxyCast<HRoadLaneConnectionProxy>(HitProx))
			{
				ConnectionUnderCursor = RoadLaneProxy->Connection;
			}
			else if(auto* LanePickerProxy = HitProxyCast<HRoadLanePickerProxy>(HitProx))
			{
				PickedUnderCursor = LanePickerProxy->LaneIndex;
			}
		}
	}

	const int SelectedActorsNum = GEditor->GetSelectedActors()->Num();
	if (SelectedActorsNum == 0)
	{
		SelectedActor = nullptr;
	}
	else
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			// "Add to Selected Actor" only targets AMetaRoad (splines on other actors are ignored).
			if (auto* Actor = Cast<AMetaRoad>(*It))
			{
				if (Actor != SelectedActor)
				{
					if (Actor->GetComponentByClass<URoadSplineComponent>())
					{
						SelectedActor = Actor;
					}
					else
					{
						SelectedActor = nullptr;
					}
					break;
				}
			}
		}
	}

	TArray<UObject*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), /*out*/ SelectedActors);

}

void UDrawRoadTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	using namespace DrawSplineToolLocals;

	Super::Render(RenderAPI);

	CashedViewToProj = MetaRoad::ViewCompat::GetWorldToClip(RenderAPI->GetSceneView()->ViewMatrices);
	CashedViewRect = RenderAPI->GetSceneView()->UnconstrainedViewRect;

	FViewCameraState NewCameraState;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(NewCameraState);

	if (!NewCameraState.Position.Equals(CameraState.Position, 50.0) ||
		!NewCameraState.Orientation.Equals(CameraState.Orientation, 0.1) ||
		!FMath::IsNearlyEqual(NewCameraState.OrthoWorldCoordinateWidth, CameraState.OrthoWorldCoordinateWidth, 1.0) ||
		!FMath::IsNearlyEqual(NewCameraState.HorizontalFOVDegrees, CameraState.HorizontalFOVDegrees, 1.0) ||
		!FMath::IsNearlyEqual(NewCameraState.AspectRatio, CameraState.AspectRatio, 0.01) ||
		NewCameraState.bIsOrthographic != CameraState.bIsOrthographic)
	{
		bConnectionsCashIsDirty = true;
		CameraState = NewCameraState;
	}

	if (PlaneMechanic)
	{
		PlaneMechanic->Render(RenderAPI);
	}

	static const float ConnectionSize = 10.0;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();


	if (WorkingSpline.IsValid() && !EndLaneConnection.IsValid())
	{
		if (bDrawTangentForLastPoint)
		{
			DrawTangent(*WorkingSpline, WorkingSpline->GetNumberOfSplinePoints() - 1, *RenderAPI);
		}

		SplineUtil::FDrawSplineSettings DrawSettings;
		SplineUtil::DrawSpline(*WorkingSpline, *RenderAPI, DrawSettings);

		for (auto& Connection : CapturedConnections)
		{
			auto& Lane = Connection->GetOwnedRoadLane();
			PDI->SetHitProxy(new HRoadLaneConnectionProxy(const_cast<ULaneConnection*>(Connection.Get()), Lane.GetStartSectionIndex(), Lane.GetLaneIndex()));
			RoadUtils::DrawRoadLaneConnection(
				ConnectionSize, Connection->IsSuccessorConnection(), false,
				Connection->CashedTransform,
				Connection == ConnectionUnderCursor ? FMetaRoadColors::SelectedColor : FColor::White,
				PDI, RenderAPI->GetSceneView(), SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
	}

	if (StartLaneConnection.IsValid())
	{
		auto& BaseLane = StartLaneConnection->GetOwnedRoadLane();
		auto& BaseLayout = StartLaneConnection->GetOwnedRoadSpline()->GetRoadLayout();
		const int SectionIndex = BaseLane.IsForwardLane() ? BaseLane.GetEndSectionIndex() : BaseLane.GetStartSectionIndex();
		const auto& [LeftLanes, RightLanes] = BaseLayout.GetLeftRighLanes(SectionIndex);
		const int BaseLaneIndex = BaseLane.GetLaneIndex();
		const int BaseLaneIndexSeq = MetaRoad::LaneIndex2SeqIndex(LeftLanes.Num(), BaseLaneIndex);

		int StartRangIndex = -1;
		int EndRangIndex = -1;

		if (BaseLaneIndex > 0)
		{
			if (PickedLeftNum != 0)
			{
				StartRangIndex = BaseLaneIndexSeq - PickedLeftNum;
				EndRangIndex = BaseLaneIndexSeq + PickedRightNum;
			}
			else if (PickedRightNum != 0)
			{
				StartRangIndex = BaseLaneIndexSeq;
				EndRangIndex = BaseLaneIndexSeq + PickedRightNum;
			}
		}
		else
		{
			if (PickedLeftNum != 0)
			{
				StartRangIndex = BaseLaneIndexSeq - PickedLeftNum + 1;
				EndRangIndex = BaseLaneIndexSeq + PickedRightNum + 1;
			}
			else if (PickedRightNum != 0)
			{
				StartRangIndex = BaseLaneIndexSeq + 1;
				EndRangIndex = BaseLaneIndexSeq + PickedRightNum + 1;
			}
		}

		int BeginIndex = 0;
		int EndIndex = 0;

		if (SectionIndex == 0 || SectionIndex == BaseLayout.Sections.Num() - 1)
		{
			BeginIndex = -LeftLanes.Num();
			EndIndex = RightLanes.Num();
		}
		else
		{
			if (BaseLane.GetLaneIndex() < 0)
			{
				BeginIndex = -LeftLanes.Num();
			}
			if (BaseLane.GetLaneIndex() > 0)
			{
				EndIndex = RightLanes.Num();
			}
		}

		for (int LaneIndex = BeginIndex; LaneIndex <= EndIndex; ++LaneIndex)
		{
			if (LaneIndex == MetaRoad::ZeroLaneIndex)
			{
				continue;
			}

			auto& Lane = LaneIndex > 0 ? RightLanes[LaneIndex - 1] : LeftLanes[-LaneIndex -1];
			const int LaneIndexSeq = MetaRoad::LaneIndex2SeqIndex(LeftLanes.Num(), LaneIndex);

			FColor Color = FColor::White;

			if (StartRangIndex != -1 && LaneIndexSeq >= StartRangIndex && LaneIndexSeq < EndRangIndex)
			{
				Color = FColor::Green;
			}

			PDI->SetHitProxy(new HRoadLanePickerProxy(LaneIndex));
			DrawLanePicker(!(Lane.IsForwardLane() ^ BaseLane.IsForwardLane()) ? Lane.SuccessorConnection : Lane.PredecessorConnection, Color, PDI, RenderAPI->GetSceneView(), SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}

		
	}
}

void UDrawRoadTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	//UE_LOG(LogMetaRoad, Log, TEXT("*** %s"), *Property->GetName());
	const FName PropertyName = Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDrawRoadToolProperties, DrawProfile))
	{
		ReCreatePreview();
	}

}

bool UDrawRoadTool::CanAccept() const
{
	return WorkingSpline.IsValid() && WorkingSpline->GetNumberOfSplinePoints() > 0;
}

// To be called by builder
void UDrawRoadTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

TSharedRef<SWidget> UDrawRoadTool::MakeShutdownOverlayWidget(const TWeakPtr<FModeToolkit>& InWeakToolkit)
{
	auto Toolkit = InWeakToolkit.Pin();
	check(Toolkit);

	// Capture a weak UObject pointer so Slate attribute getters (Image_Lambda,
	// IsEnabled_Lambda) don't dereference a stale raw pointer after GC.
	TWeakObjectPtr<UDrawRoadTool> WeakThis(this);

	auto ToolShutdownViewportOverlayWidget = SNew(SHorizontalBox)

	+SHorizontalBox::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)

			// Tool icon and name
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda([WeakThis, InWeakToolkit] () -> const FSlateBrush*
				{
					return WeakThis.IsValid() ? WeakThis->GetToolIcon(InWeakToolkit) : nullptr;
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(Toolkit->GetActiveToolDisplayName())
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("Overlay_NewActor", "Create New Actor"))
				.ToolTipText(LOCTEXT("Overlay_NewActor_Tooltip", "Create a new actor and draw a new RoadSplineComponent inside"))
				.OnClicked_Lambda([WeakThis, InWeakToolkit]()
				{
					if (WeakThis.IsValid()) WeakThis->bShutdownReqAddToSelectedActor = false;
					InWeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Accept);
					return FReply::Handled();
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("Overlay_AddToActor", "Add to Selected Actor"))
				.ToolTipText(LOCTEXT("Overlay_AddToActor_Tooltip", "Draw a new RoadSplineComponent inside a selected actor with RoadSplineComponent(s). Mainly used to create intersections and junctions"))
				.OnClicked_Lambda([WeakThis, InWeakToolkit]()
				{
					if (WeakThis.IsValid()) WeakThis->bShutdownReqAddToSelectedActor = true;
					InWeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Accept);
					return FReply::Handled();
				})
				.IsEnabled_Lambda([WeakThis]()
				{
					return WeakThis.IsValid() && WeakThis->SelectedActor.IsValid();
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.Text(LOCTEXT("Overlay_Cancel", "Cancel"))
				.ToolTipText(LOCTEXT("Overlay_Cancel_Tooltip", "Cancel the active Tool [Esc]"))
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([InWeakToolkit]()
				{
					InWeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Cancel);
					return FReply::Handled();
				})
			]
		]	
	];

	return ToolShutdownViewportOverlayWidget;
}

/*
void UDrawRoadTool::OnKeyPressed(const FKey& KeyID)
{
	bIsLeftCtrlPressed = true;
}

void UDrawRoadTool::OnKeyReleased(const FKey& KeyID)
{
	bIsLeftCtrlPressed = true;
}
*/

/// ----------------------------------------------------- Tool builder ------------------------------------------------------------------

bool UDrawRoadToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UDrawRoadToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	//InputState.SelectedActors.F

	UDrawRoadTool* NewTool = NewObject<UDrawRoadTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);

	return NewTool;
}

#undef LOCTEXT_NAMESPACE
