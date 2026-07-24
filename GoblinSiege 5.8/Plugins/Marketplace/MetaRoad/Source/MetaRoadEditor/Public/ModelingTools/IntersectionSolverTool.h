/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveToolChange.h"
#include "ToolContextInterfaces.h"
#include "TransactionUtil.h"
//#include "MetaRoadTypes.h"
#include "Utils/IntersectionFounder.h"
#include "EditorMode/ToolShutdownOverlayWidgetDefault.h"
#include "IntersectionSolverTool.generated.h"

class AActor;
class APreviewGeometryActor;
class UConstructionPlaneMechanic;
class USingleClickOrDragInputBehavior;
class URoadSplineComponent;
class UWorld;


// ---------------------------------------------------------------------------

UCLASS()
class METAROADEDITOR_API UIntersectionSolverProperties 
	: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UIntersectionSolverProperties();

};

// ---------------------------------------------------------------------------

/**
 * Intersection Solver tool.
 *
 * Automatically resolves intersection geometry and lane connectivity for road splines that
 * are connected at a junction (the automatic counterpart to the Draw Intersection tool).
 *
 * How to use:
 *   1. Select the connected road splines that form the intersection and activate the tool.
 *   2. Adjust the solver options in the Details panel (e.g. forward / reverse passes);
 *      a live preview of the solved intersection is shown.
 *   3. Accept [Enter] to generate the result, Cancel [Esc] to discard.
 */
UCLASS()
class METAROADEDITOR_API UIntersectionSolverTool
	: public UInteractiveTool
	//, public IClickBehaviorTarget
	//, public IClickDragBehaviorTarget
	//, public IHoverBehaviorTarget
	, public IToolShutdownOverlayWidgetDefault
{
	GENERATED_BODY()

public:

	virtual void SetSelectedActor(AActor* Actor);

	virtual void SetWorld(UWorld* World);
	virtual UWorld* GetTargetWorld() { return TargetWorld.Get(); }

	// UInteractiveTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IClickBehaviorTarget
	//virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos);
	//virtual void OnClicked(const FInputDeviceRay& ClickPos);

	// IClickDragBehaviorTarget
	//virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos);
	//virtual void OnClickPress(const FInputDeviceRay& PressPos);
	//virtual void OnClickDrag(const FInputDeviceRay& DragPos);
	//virtual void OnClickRelease(const FInputDeviceRay& ReleasePos);
	//virtual void OnTerminateDragSequence();

	// IHoverBehaviorTarget
	//virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	//virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	//virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	//virtual void OnEndHover() override;

private:

	UPROPERTY()
	TObjectPtr<UIntersectionSolverProperties> Settings = nullptr;


	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	// This is only used to initialize TargetActor in the settings object
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	MetaRoad::FIntersectionFounder IntersectionSolver;



private:
	UE::TransactionUtil::FLongTransactionTracker LongTransactions;

public:
	// Helper class for making undo/redo transactions, to avoid friending all the variations.
	class FSplineChange : public FToolCommandChange
	{
	public:
		// These pass the working spline to the overloads below
		virtual void Apply(UObject* Object) override;
		virtual void Revert(UObject* Object) override;

	protected:
		virtual void Apply(URoadSplineComponent& Spline) = 0;
		virtual void Revert(URoadSplineComponent& Spline) = 0;
	};
};

// ---------------------------------------------------------------------------

UCLASS(Transient)
class METAROADEDITOR_API UIntersectionSolverToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
