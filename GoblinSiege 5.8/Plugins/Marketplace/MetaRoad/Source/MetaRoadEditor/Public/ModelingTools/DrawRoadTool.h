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
#include "ToolContextInterfaces.h" // FViewCameraState
#include "TransactionUtil.h"
#include "MetaRoadTypes.h"
#include "MetaRoadEditorSettings.h"
#include "Utils/DrawUtils.h"
#include "EditorMode/ToolShutdownOverlayWidgetDefault.h"
#include "Assets/RoadProfile.h"
#include "DrawRoadTool.generated.h"

class AActor;
class AMetaRoad;
class APreviewGeometryActor;
class UConstructionPlaneMechanic;
class USingleClickOrDragInputBehavior;
class UKeyInputBehavior;
class UWorld;

namespace DrawSplineToolLocals
{
	class FSplineChange;
}

UENUM()
enum class ERoadOffsetMethod : uint8
{
	/** Spline points will be offset along the normal direction of the clicked surface */
	HitNormal,

	/** Spline points will be offset along a manually-chosen direction */
	Custom
};

/** 
 * EDrawRoadUpVectorMode
 */
UENUM()
enum class EDrawRoadUpVectorMode : uint8
{
	/** Pick the first up vector based on the hit normal, and then align subsequent up vectors with the previous ones. */
	AlignToPrevious,
	/** Base the up vector off the hit normal. */
	UseHitNormal,
};

/*
UENUM()
enum class EDrawRoad: uint8
{
	OneLane,
	Border
};
*/

// ---------------------------------------------------------------------------

UCLASS()
class METAROADEDITOR_API UDrawRoadToolProperties 
	: public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UDrawRoadToolProperties();

	/** Road draw profile presets  */
	UPROPERTY(EditAnywhere, Category = RoadSpline, meta = (EditCondition = "DrawProfileIsValid()"))
	TObjectPtr<URoadProfile> DrawProfile;


	/** Determines whether the created spline is a loop. This can be toggled using "Closed Loop" in the detail panel after spline creation. */
	UPROPERTY(EditAnywhere, Category = RoadSpline)
	bool bLoop = false;

	/** Set FRoadZone to fill the the looped spline. */
	UPROPERTY(EditAnywhere, Category = RoadSpline)
	TInstancedStruct<FRoadZone> LoopedRoadZone;

	/** How far to offset spline points from the clicked surface, along the surface normal */
	UPROPERTY(EditAnywhere, Category = Drawing, meta = (UIMin = 0, UIMax = 100))
	double ClickOffset = 20;

	/**
	 * How the spline rotation is set. It is suggested to use a nonzero FrameVisualizationWidth to see the effects.
	 */
	UPROPERTY(EditAnywhere, Category = Drawing)
	EDrawRoadUpVectorMode UpVectorMode = EDrawRoadUpVectorMode::UseHitNormal;

	/** Whether to place spline points on the surface of objects in the world */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "World Objects"))
	bool bHitWorld = true;

	/** Whether to place spline points on a custom, user-adjustable plane */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "Custom Plane"))
	bool bHitCustomPlane = false;

	/** Whether to place spline points on a plane through the origin aligned with the Z axis in perspective views, or facing the camera in othographic views */
	UPROPERTY(EditAnywhere, Category = RaycastTargets, meta = (DisplayName = "Ground Planes"))
	bool bHitGroundPlanes = true;
	/**
	 * Create the blueprint actor specified by Blueprint To Create, and either attach
	 * the spline to that, or replace an existing spline if Existing Spline Index to Replace is valid. 
	 */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bCreateBlueprint = false;

	/** Blueprint to create when Output Mode is "Create Blueprint"  */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition="bCreateBlueprint", EditConditionHides))
	TWeakObjectPtr<UBlueprint> BlueprintToCreate;

	/**
	 * If modifying a blueprint actor, whether to run the construction script while dragging or only at the end of a drag. Can be toggled off for expensive construction scripts.
	 */
	UPROPERTY(EditAnywhere, Category = Output, meta = (EditCondition="bCreateBlueprint", EditConditionHides))
	bool bRerunConstructionScriptOnDrag = true;

	TWeakObjectPtr<UDrawRoadTool> DrawRoadTool;

private:

	UFUNCTION()
	bool DrawProfileIsValid() const;
};

// ---------------------------------------------------------------------------

/**
 * Draw Road / Spline tool.
 *
 * Interactively draws a new road reference spline (URoadSplineComponent) on a surface and
 * applies the chosen Draw Profile (lane layout) to it.
 *
 * How to use:
 *   1. Left-click on the landscape or world geometry to place spline points.
 *   2. Click-drag on a point to shape its tangent (curved vs. sharp corner).
 *   3. Pick the output in the on-screen overlay: create a new actor, or add the
 *      RoadSplineComponent to the selected actor (the latter is used to build
 *      intersections and junctions inside an existing road actor).
 *   4. Accept [Enter] to commit the road, Cancel [Esc] to discard it.
 *
 * Details panel: Draw Profile, raycast targets, Blueprint output options.
 */
UCLASS()
class METAROADEDITOR_API UDrawRoadTool
	: public UInteractiveTool
	, public IClickBehaviorTarget
	, public IClickDragBehaviorTarget
	//, public IHoverBehaviorTarget
	//, public IKeyInputBehaviorTarget
	, public IToolShutdownOverlayWidget
{
	GENERATED_BODY()

	friend UDrawRoadToolProperties;
	friend DrawSplineToolLocals::FSplineChange;

public:

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
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos);
	virtual void OnClicked(const FInputDeviceRay& ClickPos);

	// IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos);
	virtual void OnClickPress(const FInputDeviceRay& PressPos);
	virtual void OnClickDrag(const FInputDeviceRay& DragPos);
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos);
	virtual void OnTerminateDragSequence();

	// IHoverBehaviorTarget
	//virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	//virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	//virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	//virtual void OnEndHover() override;

	//IKeyInputBehaviorTarget
	//virtual void OnKeyPressed(const FKey& KeyID) override;
	//virtual void OnKeyReleased(const FKey& KeyID) override;

	// IToolShutdownOverlayWidget
	virtual TSharedRef<SWidget> MakeShutdownOverlayWidget(const TWeakPtr<FModeToolkit>& InWeakToolkit);

private:

	UPROPERTY()
	TObjectPtr<UDrawRoadToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<USingleClickOrDragInputBehavior> ClickOrDragBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<UKeyInputBehavior> KeyInputBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<UConstructionPlaneMechanic> PlaneMechanic = nullptr;

	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

	// "Add to Selected Actor" target — only ever an AMetaRoad (set from a Cast<AMetaRoad> on the selection).
	TWeakObjectPtr<AMetaRoad> SelectedActor = nullptr;

	// The preview actor is either a APreviewGeometryActor with a spline, or a duplicate of 
	// some target blueprint actor so that we can see the effects of the drawn spline immediately.
	UPROPERTY()
	TObjectPtr<AActor> PreviewActor = nullptr;

	// Used for recapturing the spline when rerunning construction scripts
	int32 SplineRecaptureIndex = 0;

	// This is the spline we add points to. It points to a component nested somewhere under 
	// PreviewRootActor.
	TWeakObjectPtr<URoadSplineComponent> WorkingSpline = nullptr;

	bool bDrawTangentForLastPoint = false;

	struct FMouseTraceResult
	{
		FVector3d Location;
		FVector3d UpVector;
		FVector3d ForwardVector;
		double HitT;
		TWeakObjectPtr<ULaneConnection> Connection;
	};

	bool Raycast(const FRay& WorldRay, FVector3d& HitLocationOut, FVector3d& HitNormalOut, double& HitTOut) const;
	bool MouseTrace(const FRay& WorldRay, FMouseTraceResult& Result) const;
	void AddSplinePoint(const FVector3d& HitLocation, const FVector3d& UpVector, bool bUpdateSpline);
	FVector3d GetUpVectorToUse(const FVector3d& HitLocation, const FVector3d& HitNormal, int32 NumSplinePointsBeforehand);
	void InitRoadProfile(URoadSplineComponent * TargetSpline) const;
	void SetStartConnection(ULaneConnection* Connection);

	void ReCreatePreview();
	void GenerateAsset();

	int32 TargetActorWatcherID = -1;
	bool PreviousTargetActorVisibility = true;
	bool bPreviousSplineVisibility = true;

	bool bNeedToRerunConstructionScript = false;

	FViewCameraState CameraState;

	TWeakObjectPtr<ULaneConnection> ConnectionUnderCursor;
	TWeakObjectPtr<ULaneConnection> StartLaneConnection;
	TWeakObjectPtr<ULaneConnection> EndLaneConnection;

	int PickedUnderCursor = 0; // LaneIndex
	int PickedLeftNum = 0;
	int PickedRightNum = 0;

	//bool bCahedEnableRenderingDuringHitProxyPass = false;
	FMatrix CashedViewToProj;
	FIntRect CashedViewRect;
	bool bConnectionsCashIsDirty = false;

	bool bShutdownReqAddToSelectedActor = false;

	//bool bIsLeftCtrlPressed = false;

	UE::TransactionUtil::FLongTransactionTracker LongTransactions;

	TSet<TWeakObjectPtr<const ULaneConnection>> CapturedConnections;
};

// ---------------------------------------------------------------------------

UCLASS(Transient)
class METAROADEDITOR_API UDrawRoadToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
