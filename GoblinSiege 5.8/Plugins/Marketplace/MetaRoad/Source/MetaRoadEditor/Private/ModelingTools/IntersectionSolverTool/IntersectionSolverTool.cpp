/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "ModelingTools/IntersectionSolverTool.h"
#include "LevelEditorViewport.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "AssetSelection.h" // FActorFactoryAssetProxy
#include "BaseBehaviors/SingleClickOrDragBehavior.h"
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

#define LOCTEXT_NAMESPACE "UIntersectionSolver"

using namespace UE::Geometry;
using namespace MetaRoad;


UIntersectionSolverProperties::UIntersectionSolverProperties()
{
}

// -------------------------------------------------------------------------------------------------------------------
void UIntersectionSolverTool::Setup()
{
	UInteractiveTool::Setup();

	Settings = NewObject<UIntersectionSolverProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);


	check(TargetActor.IsValid());
	

	SetToolDisplayName(LOCTEXT("IntersectionSolverToolName", "Intersection Solver"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("IntersectionSolverToolDescription", "TODO"),
		EToolMessageLevel::UserNotification);

	TArray<TObjectPtr<const URoadSplineComponent>> Components;
	TargetActor->GetComponents(Components);

	IntersectionSolver.Setup(Components);

}


void UIntersectionSolverTool::Shutdown(EToolShutdownType ShutdownType)
{
	Super::Shutdown(ShutdownType);
}

void UIntersectionSolverTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{

}

bool UIntersectionSolverTool::CanAccept() const
{
	return true;
}

void UIntersectionSolverTool::SetSelectedActor(AActor* Actor)
{
	check(Actor);
	TargetActor = Actor;
}

void UIntersectionSolverTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UIntersectionSolverTool::FSplineChange::Apply(UObject* Object)
{
	UIntersectionSolverTool* Tool = Cast<UIntersectionSolverTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}
	//TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
	//if (!ensure(Spline.IsValid()))
	//{
	//	return;
	//}

	//Apply(*Spline);

	//Tool->bNeedToRerunConstructionScript = true;
}

void UIntersectionSolverTool::FSplineChange::Revert(UObject* Object)
{
	UIntersectionSolverTool* Tool = Cast<UIntersectionSolverTool>(Object);
	if (!ensure(Tool))
	{
		return;
	}
	//TWeakObjectPtr<URoadSplineComponent> Spline = Tool->WorkingSpline;
	//if (!ensure(Spline.IsValid()))
	//{
	//	return;
	//}

	//Revert(*Spline);

	//Tool->bNeedToRerunConstructionScript = true;
}

void UIntersectionSolverTool::OnTick(float DeltaTime)
{

}

void UIntersectionSolverTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	

	for (auto& Pt : IntersectionSolver.GetIntersections())
	{
		RenderAPI->GetPrimitiveDrawInterface()->DrawPoint(Pt, FColor::Yellow, 6, SDPG_Foreground);
	}
	
	for (auto& It : IntersectionSolver.GetSplinesData())
	{
		if (It.NodeBegin)
		{
			RenderAPI->GetPrimitiveDrawInterface()->DrawPoint(It.Spline->GetLocationAtDistanceAlongSpline(It.NodeBegin->SOffset + (It.NodeBegin->Dir == FIntersectionFounder::ENodeDir::Forward ? -500: 500), ESplineCoordinateSpace::World), FColor::White, 10, SDPG_Foreground);
		}
		if (It.NodeEnd)
		{
			RenderAPI->GetPrimitiveDrawInterface()->DrawPoint(It.Spline->GetLocationAtDistanceAlongSpline(It.NodeEnd->SOffset + (It.NodeEnd->Dir == FIntersectionFounder::ENodeDir::Forward ? -500 : 500), ESplineCoordinateSpace::World), FColor::White, 10, SDPG_Foreground);
		}
	}

}


/// ----------------------------------------------------- Tool builder ------------------------------------------------------------------



bool UIntersectionSolverToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	if (AActor* Actor = ToolBuilderUtil::FindFirstActor(SceneState, [](AActor*) { return true; }))
	{

		TInlineComponentArray<URoadSplineComponent*> SplineComponent;
		Actor->GetComponents(SplineComponent);

		return SplineComponent.Num() >= 2;
	}
	else
	{
		return false;
	}
}

UInteractiveTool* UIntersectionSolverToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UIntersectionSolverTool* NewTool = NewObject<UIntersectionSolverTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);

	// May be null
	NewTool->SetSelectedActor(ToolBuilderUtil::FindFirstActor(SceneState, [](AActor*) { return true; }));

	return NewTool;
}


#undef LOCTEXT_NAMESPACE
