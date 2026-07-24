/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "MetaRoadTypes.h"
#include "EditorMode/IToolShutdownOverlayWidget.h"
#include "Assets/RoadProfile.h"
#include "DrawRoundaboutTool.generated.h"

class APreviewGeometryActor;
class UClickDragInputBehavior;
class UWorld;

// ---------------------------------------------------------------------------

UCLASS()
class METAROADEDITOR_API UDrawRoundaboutToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UDrawRoundaboutToolProperties();

	UPROPERTY(EditAnywhere, Category = Roundabout)
	TObjectPtr<URoadProfile> DrawProfile;

	UPROPERTY(EditAnywhere, Category = Roundabout)
	TInstancedStruct<FRoadZone> LoopedRoadZone;

	/** Ring radius in centimeters. Updated live during drag; editable afterwards. */
	UPROPERTY(EditAnywhere, Category = Roundabout, meta = (ClampMin = "200.0"))
	double Radius = 1000.0;

	/** Number of spline control points (2–16). More points produce a smoother circle. */
	UPROPERTY(EditAnywhere, Category = Roundabout, meta = (ClampMin = "2", ClampMax = "16"))
	int32 NumPoints = 4;

	/** Rotation of the first spline point around the center. */
	UPROPERTY(EditAnywhere, Category = Roundabout,
		meta = (UIMin = "-180.0", UIMax = "180.0", ClampMin = "-180.0", ClampMax = "180.0"))
	double Rotation = 0.0;

	/** Spline traversal direction: true = clockwise (CW), false = counter-clockwise (CCW). */
	UPROPERTY(EditAnywhere, Category = Roundabout)
	bool bClockwise = false;

	UPROPERTY(EditAnywhere, Category = Raycast, meta = (DisplayName = "World Objects"))
	bool bHitWorld = true;

	UPROPERTY(EditAnywhere, Category = Raycast, meta = (DisplayName = "Ground Plane"))
	bool bHitGroundPlane = true;

	UPROPERTY(EditAnywhere, Category = Raycast, meta = (UIMin = 0, UIMax = 100))
	double ClickOffset = 20.0;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bCreateBlueprint = false;

	UPROPERTY(EditAnywhere, Category = Output,
		meta = (EditCondition = "bCreateBlueprint", EditConditionHides))
	TWeakObjectPtr<UBlueprint> BlueprintToCreate;
};

// ---------------------------------------------------------------------------

/**
 * Draw Roundabout tool.
 *
 * Draws a closed circular road spline (roundabout / ring road).
 *
 * How to use:
 *   1. Press LMB at the desired center and drag outward to set the radius.
 *   2. Release LMB to fix the spline; a live wireframe preview is shown while dragging.
 *   3. Fine-tune Radius / NumPoints / Rotation / Draw Profile in the Details panel;
 *      LoopedRoadZone fills the interior island (e.g. a pedestrian island).
 *   4. Accept [Enter] to commit, Cancel [Esc] to discard.
 */
UCLASS()
class METAROADEDITOR_API UDrawRoundaboutTool
	: public UInteractiveTool
	, public IClickDragBehaviorTarget
	, public IToolShutdownOverlayWidget
{
	GENERATED_BODY()
public:
	void SetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	// IToolShutdownOverlayWidget
	virtual TSharedRef<SWidget> MakeShutdownOverlayWidget(
		const TWeakPtr<FModeToolkit>& InWeakToolkit) override;

private:
	bool Raycast(const FRay& WorldRay, FVector& OutLocation) const;
	void RebuildCircleSpline();
	void ApplyProfileToSpline(URoadSplineComponent* Spline) const;
	void CreatePreviewActor();
	void GenerateAsset();

	UPROPERTY() TObjectPtr<UDrawRoundaboutToolProperties> Settings     = nullptr;
	UPROPERTY() TObjectPtr<UClickDragInputBehavior>       DragBehavior = nullptr;
	UPROPERTY() TObjectPtr<AActor>                        PreviewActor = nullptr;

	TWeakObjectPtr<UWorld>               TargetWorld;
	TWeakObjectPtr<URoadSplineComponent> WorkingSpline;

	FVector CircleCenter                   = FVector::ZeroVector;
	bool    bIsDragging                    = false;
	bool    bSplineReady                   = false;
	bool    bShutdownReqAddToSelectedActor = false;
};

// ---------------------------------------------------------------------------

UCLASS(Transient)
class METAROADEDITOR_API UDrawRoundaboutToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
