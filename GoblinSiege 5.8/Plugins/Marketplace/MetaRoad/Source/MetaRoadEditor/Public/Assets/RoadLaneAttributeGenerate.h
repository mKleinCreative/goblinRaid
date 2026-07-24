/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/CollisionProfile.h"
#include "RoadLaneAttributeGenerate.generated.h"

struct FReferenceSplineMeshParams;

/** Controls the anchor point used for lane-width mapping and LeftWidth/RightWidth calculation.
 *  Auto: anchor = geometric center of the lane/road; Alpha is ignored for positioning.
 *  Fixed: anchor driven by FRoadLaneAttributeGenerateValue::Alpha (0=inner edge, 0.5=center, 1=outer). */
UENUM(BlueprintType)
enum class ERoadLaneAlignment : uint8
{
	Fixed  UMETA(DisplayName = "Fixed (Use Alpha)"),
	Auto   UMETA(DisplayName = "Auto (Geometric Center)"),
};

/**
 * Base value struct for curve-like lane attributes (SplineMesh, Component, Actor, Lofting).
 *
 * Each key of a FRoadLaneAttribute holds one instance of this struct (or a subclass).
 * The central concept is the **anchor point** — the R-coordinate on the road surface where
 * X=0 of the generated cross-section or mesh lands:
 *
 *   Alignment == Auto  → anchor = geometric centre of the lane or road zone
 *   Alignment == Fixed → anchor driven by Alpha:
 *     · Non-centre lane: LeftR + Alpha × LaneWidth
 *     · Centre lane (ZeroLaneIndex) piecewise through road reference (R=0):
 *         Alpha=0.0 → LeftR, Alpha=0.5 → R=0, Alpha=1.0 → RightR
 *
 * Cubic-interpolated fields (Alpha, Scale, Offset, Roll) are blended between adjacent keys
 * via FRoadLaneAttributeGenerateValue::Interpolate(). All other fields are stepped.
 *
 * Subclasses (e.g. FRoadLaneAttributeLoftingValue) add their own payload and override
 * Interpolate() — they must NOT call Super::Interpolate() and must manually copy every
 * stepped field from the base (Alignment, bIsReverse, etc.).
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROADEDITOR_API FRoadLaneAttributeGenerateValue : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadLaneAttributeGenerateValue() {}

	/** Lateral position within the lane: 0 = inner edge, 0.5 = center, 1 = outer edge.
	 *  Ignored when Alignment == Auto (anchor is always the geometric centre). */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (UIMin = -5.0, ClampMin = -5.0, UIMax = 5.0, ClampMax = 5.0, EditCondition = "Alignment == ERoadLaneAlignment::Fixed", EditConditionHides))
	double Alpha = 0.5;

	/** Per-axis scale of the cross-section shape (X = lateral, Y = vertical). */
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	FVector2D Scale = { 1.0, 1.0 };

	/** Translation of the cross-section relative to the road surface [cm] (X = lateral / R, Y = vertical / H). */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (ForceUnits = "cm"));
	FVector2D Offset = {};

	/** Rotation of the cross-section around the road S-axis [degrees]. */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (ForceUnits = "Degrees"));
	double Roll = 0.0;

	/** Mirrors the cross-section along its lateral axis (left ↔ right flip).
	 *  All adjacent attribute segments sharing a continuous strip must use the same value. */
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	bool bIsReverse = false;

	/** If true, this key's segment is skipped and any active strip is flushed. */
	UPROPERTY(EditAnywhere, Category = AttributeKey);
	bool bSkipSegment = false;

	/** Controls where X=0 of the cross-section profile is anchored within the lane/road width,
	 *  and how LeftWidth/RightWidth are computed for FReferenceSplineMeshParams.
	 *  Auto: geometric center of the lane (Alpha ignored for positioning).
	 *  Fixed: position given by Alpha (0=inner edge, 0.5=center, 1=outer edge). */
	UPROPERTY(EditAnywhere, Category = AttributeKey)
	ERoadLaneAlignment Alignment = ERoadLaneAlignment::Fixed;

	/** If true, LeftWidth overrides the computed left extent from the anchor point. */
	UPROPERTY(EditAnywhere, Category = AttributeKey)
	bool OverrideLeftWidth = false;

	/** If true, RightWidth overrides the computed right extent from the anchor point. */
	UPROPERTY(EditAnywhere, Category = AttributeKey)
	bool OverrideRightWidth = false;

	/** Override for the distance [cm] from the anchor to the left zone boundary.
	 *  Active when OverrideLeftWidth = true. */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (EditCondition = "OverrideLeftWidth", ClampMin = 0.0, ForceUnits = "cm"))
	double LeftWidth = 500.0;

	/** Override for the distance [cm] from the anchor to the right zone boundary.
	 *  Active when OverrideRightWidth = true. */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (EditCondition = "OverrideRightWidth", ClampMin = 0.0, ForceUnits = "cm"))
	double RightWidth = 500.0;

	virtual bool Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* Other, float Alpha, FRoadLaneAttributeValue* Out) const override;
	virtual bool CanInterpolate() const { return true; }
	virtual bool SupportsAlphaEditing() const override { return true; }
	virtual bool SetKeyAlpha(double InAlpha) override
	{
		Alpha = FMath::Clamp(InAlpha, 0.0, 1.0);
		return true;
	}
	virtual double GetKeyAlpha() const override { return (Alignment == ERoadLaneAlignment::Auto) ? 0.5 : Alpha; }
	virtual double GetCenterLaneROffset(double InLeftR, double InRightR) const override;
	virtual bool SetAlphaFromROffset(double AnchorR, double LaneInnerR, double LaneOuterR, bool bIsZeroLane) override;
};

/**
 * Road-aware per-segment parameters for a curve attribute, produced by FSplineMeshOp and consumed
 * by URoadLaneAttributeGenerateDescriptor::GenerateAsset().
 *
 * Population pipeline (all inside FSplineMeshOp::CalculateResult):
 *   1. MakePolylineSpline()        — traces the anchor path via GetRoadPosition(), taking
 *                                    Alignment and Alpha into account. For ZeroLaneIndex
 *                                    uses the 2-arg GetRoadPosition(SOffset, TotalR) because the
 *                                    4-arg overload ignores Alpha for the centre lane.
 *   2. ComputeWidths()             — derives (LeftWidth, RightWidth) from EvalLaneROffset();
 *                                    OverrideLeftWidth / OverrideRightWidth bypass road data.
 *   3. CompletePolyline()          — Arc-length-interpolates Scale/Offset/Roll/Widths on vertices
 *                                    that fall between attribute keys.
 *   4. BuildPolylineSplineCurves() — Converts the vertex list to FSplineCurves (arc-length
 *                                    re-parametrization) + per-field FInterpCurve objects.
 *   5. MakeSegments()              — Subdivides into N segments of ≈LengthOfSegment cm.
 *                                    StartPos/EndPos/Tangents via FitHermiteTangents();
 *                                    StartRoll/EndRoll from road-surface normal + Roll field.
 *
 * StartScale / EndScale ← FRoadLaneAttributeGenerateValue::Scale (cubic-interpolated)
 * StartOffset / EndOffset ← FRoadLaneAttributeGenerateValue::Offset (cubic-interpolated)
 * Start/EndLeftWidth, Start/EndRightWidth ← ComputeWidths() or OverrideLeft/RightWidth
 *
 * Extends FSplineMeshParams with Left/RightWidth fields and Profile pointer.
 * Convertible to/from FSplineMeshParams via constructor and cast operator.
 */
USTRUCT(BlueprintType)
struct METAROADEDITOR_API FReferenceSplineMeshParams
{
	GENERATED_USTRUCT_BODY()

	FReferenceSplineMeshParams() = default;
	FReferenceSplineMeshParams(const FSplineMeshParams& Other);

	/** Start location of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector StartPos{};

	/** Start tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector StartTangent{};

	/** X and Y scale applied to mesh at start of spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D StartScale{ 1.0 };

	/** Roll around spline applied at start, in radians. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	float StartRoll{};

	/** Roll around spline applied at end, in radians. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay, meta = (DisplayAfter = "EndTangent"))
	float EndRoll{};

	/** Starting offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D StartOffset{};

	/** End location of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector EndPos{};

	/** X and Y scale applied to mesh at end of spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D EndScale{ 1.0 };

	/** End tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	FVector EndTangent{};

	/** Ending offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh, AdvancedDisplay)
	FVector2D EndOffset{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	bool bAlignWorldUpVector = false;

	/** Distance [cm] from the attribute position to the left lane/road boundary at segment start.
	 *  Center lane (ZeroLaneIndex): distance from road reference line to the leftmost road edge.
	 *  Per-lane: Alpha * LaneWidth (distance from attribute to the inner/left lane boundary). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	float StartLeftWidth = 0.f;

	/** Distance [cm] from the attribute position to the right lane/road boundary at segment start.
	 *  Center lane: distance from road reference to the rightmost road edge.
	 *  Per-lane: (1 - Alpha) * LaneWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	float StartRightWidth = 0.f;

	/** Distance [cm] from the attribute position to the left lane/road boundary at segment end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	float EndLeftWidth = 0.f;

	/** Distance [cm] from the attribute position to the right lane/road boundary at segment end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SplineMesh)
	float EndRightWidth = 0.f;

	operator FSplineMeshParams() const;
};

/** Controls how the polyline is divided into GenerateAsset() calls. */
UENUM(BlueprintType)
enum class ERoadLaneAttributeGenerateSampler : uint8
{
	/** Splits total arc-length into N ≈ LengthOfSegment intervals (default) */
	ByLengthOfSegment  UMETA(DisplayName = "By Length of Segment"),

	/** One segment per adjacent attribute key pair; LengthOfSegment is ignored.
	 *  N keys → N-1 segments. Use ComponentToSegmentAlign/ActorToSegmentAlign
	 *  to position the object within each key interval. */
	BetweenKeys        UMETA(DisplayName = "Between Keys"),
};

/**
 * Abstract base for attribute descriptors that generate geometry following a road lane via FSplineMeshOp.
 *
 * FSplineMeshOp reads FRoadLaneAttributeGenerateValue keys from the lane, builds a set of
 * FReferenceSplineMeshParams (one per LengthOfSegment), and calls GenerateAsset() once per
 * segment on the descriptor instance. Subclasses implement GenerateAsset() to create the
 * concrete UE object (USplineMeshComponent, USceneComponent subclass, or AActor).
 *
 * Blueprint-implementable via ReceiveGenerateAsset(); C++ subclasses override GenerateAsset() directly.
 *
 * Static helpers for Blueprint:
 *   CalcSliceTransformAtSplineOffset() — world transform at a fractional point along a segment
 *   CalcWidthsAtSplineOffset()         — interpolated (LeftWidth, RightWidth) at a fractional point
 */
UCLASS(Abstract, BlueprintType, Blueprintable, DisplayName = "Generate")
class METAROADEDITOR_API URoadLaneAttributeGenerateDescriptor : public URoadLaneAttributeDescriptor
{
	GENERATED_BODY()

public:

	/** Default attribute value applied to the lane. Acts as the initial key when no keys exist,
	 *  and determines Alignment/Alpha used for anchor placement by FSplineMeshOp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FRoadLaneAttributeGenerateValue AttributeValueTemplate{};

	/** Target arc-length of each generated segment [cm]. FSplineMeshOp subdivides the lane span
	 *  into N segments of approximately this length; the last segment may be shorter.
	 *  Ignored when Sampler == BetweenKeys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute, meta = (UIMin = 1.0, ClampMin = 1.0, EditCondition = "Sampler == ERoadLaneAttributeGenerateSampler::ByLengthOfSegment"))
	double LengthOfSegment = 1500;

	/** Controls how the polyline is subdivided into GenerateAsset() calls.
	 *  ByLengthOfSegment: N segments of ≈LengthOfSegment cm (default, good for spline meshes).
	 *  BetweenKeys: one segment per adjacent attribute key pair (good for point objects). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	ERoadLaneAttributeGenerateSampler Sampler = ERoadLaneAttributeGenerateSampler::ByLengthOfSegment;

	/** When true, the up-vector used for the spline cross-section frame is locked to world Z
	 *  instead of the road surface normal. Useful for vertical elements (poles, signs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	bool bAlignWorldUpVector = false;

	/** Reverses the direction of travel along the spline so the mesh faces the opposite way.
	 *  Use when the asset's forward axis points against the road S-direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	bool bReversSplineDirection = false;

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const override { return AttributeValueTemplate; }

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const { ReceiveGenerateAsset(SplineMeshParams, TargetActor, bIsPreview); }

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GenerateAsset"))
	void ReceiveGenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const;

	/** World transform at fractional position Alpha along the segment [MinT, MaxT].
	 *  Applies StartOffset/EndOffset and StartRoll/EndRoll interpolation identically to
	 *  USplineMeshComponent. Use for placing sub-objects (bolts, joints) at a known point. */
	UFUNCTION(BlueprintCallable, Category = "CustomSplineBuilder")
	static FTransform CalcSliceTransformAtSplineOffset(const FReferenceSplineMeshParams& SplineMeshParams, const float Alpha, const float MinT = 0.0, const float MaxT = 1.0);

	/** Interpolates LeftWidth and RightWidth at a given position along the segment.
	 *  Returns FVector2D(LeftWidth, RightWidth). Alpha is in [MinT, MaxT], clamped. */
	UFUNCTION(BlueprintCallable, Category = "CustomSplineBuilder")
	static FVector2D CalcWidthsAtSplineOffset(const FReferenceSplineMeshParams& SplineMeshParams, const float Alpha, const float MinT = 0.0, const float MaxT = 1.0);

};

/**
 * Places a USplineMeshComponent along the road lane for each generated segment.
 * Typical use: guardrails, barriers, cable ducts — any repeating mesh that bends with the road.
 *
 * GenerateAsset() creates a new USplineMeshComponent on TargetActor, applies StaticMesh,
 * copies FReferenceSplineMeshParams → USplineMeshComponent::SplineMeshParams, and configures
 * collision from BodyInstance.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, DisplayName = "Spline Mesh")
class METAROADEDITOR_API URoadLaneAttributeSplineMeshDescriptor : public URoadLaneAttributeGenerateDescriptor
{
	GENERATED_BODY()

public:
	URoadLaneAttributeSplineMeshDescriptor()
	{
		BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	TObjectPtr<class UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FBodyInstance BodyInstance;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const override;
};

/**
 * Places an instance of any USceneComponent subclass along the road lane once per segment.
 * ComponentTemplate specifies the component class to instantiate.
 * If ComponentTemplate is a USplineMeshComponent, FReferenceSplineMeshParams are applied to it.
 * ComponentToSegmentAlign [0,1] controls the placement point within the segment:
 *   0 = segment start, 0.5 = midpoint, 1 = segment end.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, DisplayName = "Spline Component")
class METAROADEDITOR_API URoadLaneAttributeComponentTemplateDescriptor : public URoadLaneAttributeGenerateDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	TSubclassOf<class USceneComponent> ComponentTemplate;

	/** Fractional position within the segment [0=start, 0.5=mid, 1=end] where the component is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0))
	double ComponentToSegmentAlign = 0.0;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const override;
};

/**
 * Spawns an AActor subclass once per generated segment along the road lane.
 * Actor specifies the class to spawn. ActorToSegmentAlign [0,1] picks the segment position:
 *   0 = segment start, 0.5 = midpoint, 1 = segment end (via CalcSliceTransformAtSplineOffset).
 * The spawned actor is attached to TargetActor.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, DisplayName = "Spline Actor")
class METAROADEDITOR_API URoadLaneAttributeActortTemplateDescriptor : public URoadLaneAttributeGenerateDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	TSubclassOf<class AActor> Actor;

	/** Fractional position within the segment [0=start, 0.5=mid, 1=end] where the actor is spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0))
	double ActorToSegmentAlign = 0.0;

	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, AActor* TargetActor, bool bIsPreview) const override;
};




