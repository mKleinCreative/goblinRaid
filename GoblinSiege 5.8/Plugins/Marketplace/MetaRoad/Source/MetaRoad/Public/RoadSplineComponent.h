/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Components/SplineComponent.h"
#include "MetaRoadTypes.h"
#include "Templates/Tuple.h"
#include "RoadSplineComponent.generated.h"

struct FDriveSplineInstanceData;


enum class EComputeArcMode
{
	AdjStartTangent,
	AdjEndTangent,
	AdjStartPos,
	AdjEndPos
};


UENUM(BlueprintType)
enum class ERoadSplinePointType: uint8
{
	Linear = ESplinePointType::Linear,
	Curve = ESplinePointType::Curve,
	Constant = ESplinePointType::Constant,
	CurveClamped = ESplinePointType::CurveClamped,
	CurveCustomTangent = ESplinePointType::CurveCustomTangent,
	Arc,
	Broken   // CurveUser point whose Arrive/Leave tangents are edited independently (sharp corner)
};

UENUM(BlueprintType)
enum class ERoadSplinePointTypeOverride : uint8
{
	Inherited,
	Arc,
	Broken   // implies CIM_CurveUser
};

UENUM(BlueprintType)
enum class ERoadSplineMagicTransformFilter: uint8
{
	InnrerOnly, // Update all connected splines in the owned actor only
	OuterOnly,  // Update all connected splines in all actores except owned actor
	All // Update all connected splines
};


/**
 *  Height blending algorithm in case of intersection of two or more splines
 */
UENUM(BlueprintType)
enum class ERoadLandscapeBlendMode : uint8
{
	Default,
	Maximum, // Lower priority: applied first; Legacy splines overwrite its pixels. At the intersection of one or more road splines, the maximum height will be selected.
	Legacy   // Higher priority: applied last; always wins over Maximum at overlapping pixels. Just like standard spline blending in UE landscape (see ELandscapeBlendMode::LSBM_AlphaBlend).
};


/**
 * URoadSplineMetadata
 */
UCLASS()
class METAROAD_API URoadSplineMetadata : public USplineMetadata
{
	GENERATED_UCLASS_BODY()

public:
	/** Insert point before index, lerping metadata between previous and next key values */
	virtual void InsertPoint(int32 Index, float t, bool bClosedLoop) override;
	/** Update point at index by lerping metadata between previous and next key values */
	virtual void UpdatePoint(int32 Index, float t, bool bClosedLoop) override;
	virtual void AddPoint(float InputKey) override;
	virtual void RemovePoint(int32 Index) override;
	virtual void DuplicatePoint(int32 Index) override;
	virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	virtual void Reset(int32 NumPoints) override;
	virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;

	TWeakObjectPtr<URoadSplineComponent> Spline;

	/** Per-point spline type override (one entry per spline point). Owned and maintained here:
	 *  the engine/visualizer drives the virtual point hooks above to keep it in sync. */
	UPROPERTY()
	TArray<ERoadSplinePointTypeOverride> PointTypes;

};

USTRUCT(BlueprintType)
struct FRoadPosition
{
	GENERATED_BODY()

	/** World or local position on the road surface */
	UPROPERTY(BlueprintReadWrite, Category = RoadPosition)
	FVector Location = {};

	/** Orientation aligned with the lane direction (forward = lane direction) */
	UPROPERTY(BlueprintReadWrite, Category = RoadPosition)
	FQuat Quat = {};

	/** Arc-length distance from spline start */
	UPROPERTY(BlueprintReadWrite, Category = RoadPosition)
	double SOffset = 0;  

	/** Lateral distance from the reference spline (positive = right) */
	UPROPERTY(BlueprintReadWrite, Category = RoadPosition)
	double ROffset = 0; 

};

USTRUCT(BlueprintType)
struct FRoadHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Road)
	FVector HitPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = Road)
	int32 SectionIndex = INDEX_NONE;

	/** LaneIndex > 0 = right lane, < 0 = left lane, 0 = MetaRoad::ZeroLaneIndex (no lane hit) */
	UPROPERTY(BlueprintReadOnly, Category = Road)
	int32 LaneIndex = MetaRoad::ZeroLaneIndex;
};

/**
 * URoadSplineComponent
 */
UCLASS(BlueprintType, Blueprintable, ShowCategories = ("Rendering"), meta = (BlueprintSpawnableComponent))
class METAROAD_API URoadSplineComponent 
	: public USplineComponent
{
	GENERATED_UCLASS_BODY()
	
	friend URoadSplineMetadata;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Road)
	FRoadLayout RoadLayout;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Export, Category = Road)
	TObjectPtr<URoadConnection> PredecessorConnection;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Export, Category = Road)
	TObjectPtr<URoadConnection> SuccessorConnection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape)
	TObjectPtr<class ALandscape> Landscape;

	/**
	 *  Height blending algorithm in case of intersection of two or more splines.
	 */
	UPROPERTY(EditAnywhere, Category = Landscape)
	ERoadLandscapeBlendMode LandscapeBlendMode = ERoadLandscapeBlendMode::Default;

	/**
	 * Name of blend layer to paint when applying spline to landscape.
	 * If "none", no layer is painted.
	 */
	UPROPERTY(EditAnywhere, Category = Landscape)
	FName LandscapeLayerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float LandscapeLayerFactor = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	float LandscapeSideFalloff = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))

	float LandscapeSideOffset = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	float LandscapeResolution = 512;

	// Z Offset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Landscape, meta = (UIMin = 0, ClampMin = 0))
	double LandscapeOffset = 50;

public:
	/** Skip procedural generation for this spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Road)
	bool bSkipProceduralGeneration = false;

	/** Material priority override for procedural generation. Higher value wins when zones overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Road)
	uint8 MaterialPriority = 0;

	/** Triangulation sub-group within this actor. Splines with the same SubGroup triangulate together
	 *  independently from other sub-groups; all sub-groups share one _Gen output actor.
	 *  NAME_None = default sub-group (backward compatible with single-group actors). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Road)
	FName SubGroup;

public:
	virtual ~URoadSplineComponent();

	virtual USplineMetadata* GetSplinePointsMetadata() { return SplineMetadata; }
	virtual const USplineMetadata* GetSplinePointsMetadata() const { return SplineMetadata; }

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, CallInEditor, Category = Landscape)
	void UpdateLandscape();
#endif

	/** Rebuilds FRoadLayout back-pointers (SectionIndex, LaneIndex, OwnedRoadLayout) for all
	 *  sections and lanes. Call after structural changes to RoadLayout.Sections[]. */
	UFUNCTION(BlueprintCallable, Category = Road)
	void UpdateRoadLayout();

	/** Recomputes SOffsetEnd_Cashed for all sections and lanes based on current spline length. */
	UFUNCTION(BlueprintCallable, Category = Road)
	void UpdateLaneSectionBounds();

	/** Clips Width curves and Attributes for all lanes to their section's SOffset bounds,
	 *  then removes sections that fall entirely outside the spline length. */
	UFUNCTION(BlueprintCallable, Category = Road)
	void TrimLaneSections(double Tolerance = 0.1);

	/** Propagates a transform change to all connected URoadSplineComponents so that
	 *  endpoints sharing a connection stay geometrically aligned.
	 *  Filter controls which connected splines (inner actor / outer actors / all) receive the update. */
	UFUNCTION(BlueprintCallable, Category = Spline)
	void UpdateMagicTransform(ERoadSplineMagicTransformFilter Filter = ERoadSplineMagicTransformFilter::All);

	UFUNCTION(BlueprintCallable, Category = Spline)
	virtual void UpdateAutoTangents(int EditingPointIndex = -1);

	//UFUNCTION(BlueprintCallable, Category = Spline)
	virtual void UpdateSpline(int EditingPointIndex);

	UFUNCTION(BlueprintCallable, Category = Road)
	const FRoadLayout& GetRoadLayout() const { return RoadLayout; }
	FRoadLayout& GetRoadLayout() { return RoadLayout; }

	UFUNCTION(BlueprintCallable, Category = Road)
	const TArray<FRoadLaneSection> & GetLaneSections() const { return RoadLayout.Sections; }
	TArray<FRoadLaneSection>& GetLaneSections() { return RoadLayout.Sections; }

	UFUNCTION(BlueprintCallable, Category = Road)
	const FRoadLaneSection & GetLaneSection(int i) const { return RoadLayout.Sections[i]; }
	FRoadLaneSection& GetLaneSection(int i) { return RoadLayout.Sections[i]; }

	UFUNCTION(BlueprintCallable, Category = Road)
	int GetLaneSectionsNum() const { return RoadLayout.Sections.Num(); }

	const FRoadLane* GetRoadLane(int SectionIndex, int LaneIndex) const;
	FRoadLane* GetRoadLane(int SectionIndex, int LaneIndex);

	//const TArray<ERoadSplinePointTypeOverride>& GetRoadPointTypes() const { return PointTypes; }

	UFUNCTION(BlueprintCallable, Category = Spline)
	ERoadSplinePointType GetRoadSplinePointType(int32 PointIndex) const;

	UFUNCTION(BlueprintCallable, Category = Spline)
	void SetRoadSplinePointType(int32 PointIndex, ERoadSplinePointType Mode, bool bUpdateSpline = true);

	/** Returns true if the point uses independent Arrive/Leave tangents (Broken type). */
	UFUNCTION(BlueprintCallable, Category = Spline)
	bool IsRoadSplinePointBroken(int32 PointIndex) const;

	void ApplyComponentInstanceData(struct FDriveSplineInstanceData* ComponentInstanceData, const bool bPostUCS);

	/** Builds a copy of this spline's curves shifted laterally by RightOffset (positive = right).
	 *  Used internally to compute left/right road edge splines for mesh generation. */
	void BuildOffsetCurves(double RightOffset, FSplineCurves& OutCurves) const;

	/** Converts the spline segment [S0, S1] to an adaptively sampled polyline.
	 *  @param RightOffsetFunc       Function returning the R-offset at each SOffset
	 *  @param ReparamStepsPerSegment  Samples per spline segment for reparameterisation
	 *  @param MinNumSteps           Minimum output points regardless of spline length */
	void BuildLinearApproximation(TArray<FSplinePositionLinearApproximation>& OutPoints, const TFunction<double(double)>& RightOffsetFunc, double S0, double S1, int ReparamStepsPerSegment, int MinNumSteps, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Returns the world/local position of lane LaneIndex at arc-length S.
	 *  Alpha selects the lateral edge: 0.0 = inner edge (nearest center), 1.0 = outer edge. */
	FVector EvalLanePoistion(int SectionIndex, int LaneIndex, double S, double Alpha, ESplineCoordinateSpace::Type CoordinateSpace) const;

	double EvalROffset(double S) const;

	struct FRang { double StartS; double EndS; };
	/** Returns the [StartS, EndS] arc-length range covered by the given lane.
	 *  The lane may start after SOffset=0 if its owning section starts later. */
	FRang GetLaneRang(int SectionIndex, int LaneIndex) const;

	//  Find closest points between spline segments (Key1 and Key2) and linear segment (A1 and B2A2 return spline kay
	float ClosetsKeyToSegmant(float Key1, float Key2, const FVector& A1, const FVector& A2) const;

	//  Find closest points between spline segments (S1 and S2) and linear segment (A1 and A2) return spline kay
	float ClosetsKeyToSegmant2(float S1, float S2, const FVector& A1, const FVector& A2) const;

	// Finds the spline key at the road-surface point hit by the click ray [RayStart, RayEnd].
	// More accurate than ClosetsKeyToSegmant2 for oblique camera angles: intersects the ray with
	// the road surface plane (road up vector as normal) rather than computing minimum 3D segment
	// distance. Falls back to ClosetsKeyToSegmant2 when the ray is nearly parallel to the surface.
	float KeyAtRayHit(float S1, float S2, const FVector& RayStart, const FVector& RayEnd) const;

	URoadConnection* GetPredecessorConnection() const { return PredecessorConnection; }
	URoadConnection* GetSuccessorConnection() const { return SuccessorConnection; }

	/** Traverses connections to find all ULaneConnections reachable at the end of the given lane.
	 *  bIncludesThisRoad — if true, includes connections on this spline itself. */
	TArray<ULaneConnection*> FindAllSuccessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad = false) const;
	/** Traverses connections to find all ULaneConnections reachable at the start of the given lane.
	 *  bIncludesThisRoad — if true, includes connections on this spline itself. */
	TArray<ULaneConnection*> FindAllPredecessors(int SectionIndex, int LaneIndex, bool bIncludesThisRoad = false) const;

	/** Returns the index of the section whose SOffset range contains SplineKey.
	 *  Returns INDEX_NONE if SplineKey is outside all sections. */
	int FindRoadSectionOnSplineKey(float SplineKey) const;


	/** Inserts a new section boundary at SplineKey, splitting the existing section at that point.
	 *  Side restricts which side (Left / Right / Both) gets the new boundary.
	 *  Returns the index of the newly created section, or INDEX_NONE on failure. */
	virtual int SplitSection(float SplineKey, ERoadLaneSectionSide Side);

	virtual void DisconnectAll();

	void SetSelectedLane(int InSectionIndex, int InLaneSectionIndex) { SelectedSectionIndex = InSectionIndex; SelectedLaneSectionIndex = InLaneSectionIndex; };
	TTuple<int, int> GetSelectedLane() const { return { SelectedSectionIndex , SelectedLaneSectionIndex }; }

	/** Auxiliary multi-selection set used by the editor to highlight several lanes at once.
	 *  The primary lane (SetSelectedLane/GetSelectedLane) is always one of these when set.
	 *  Transient render cache, mirrors SelectedSectionIndex (not serialized).
	 *
	 *  Threading: written on the game thread (editor selection) and read live on the render thread by
	 *  FRoadSplineSceneProxy::GetDynamicMeshElements via IsLaneSelected(). This matches the pre-existing
	 *  live-read pattern of GetSelectedLane(); selection edits are infrequent, lane counts tiny, and each
	 *  edit is followed by a redraw. If lane selection ever becomes high-frequency, push a copy of the set
	 *  to the scene proxy via a render command instead of reading the array live. */
	void SetSelectedLanes(const TArray<TTuple<int, int>>& InSelectedLanes) { SelectedLanes = InSelectedLanes; }
	void ClearSelectedLanes() { SelectedLanes.Reset(); bLoopSelected = false; }
	bool IsLaneSelected(int InSectionIndex, int InLaneSectionIndex) const
	{
		if (InSectionIndex == SelectedSectionIndex && InLaneSectionIndex == SelectedLaneSectionIndex)
		{
			return true;
		}
		return SelectedLanes.Contains(TTuple<int, int>(InSectionIndex, InLaneSectionIndex));
	}

	/** Editor highlight for the looped fill area of a closed spline (transient render cache, like the lane
	 *  selection above). Read live on the render thread by FRoadSplineSceneProxy::GetDynamicMeshElements. */
	void SetLoopSelected(bool bInSelected) { bLoopSelected = bInSelected; }
	bool IsLoopSelected() const { return bLoopSelected; }

	FQuat GetBackwardQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FTransform GetBackwardTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale=false) const;
	
	/** Fixed version of origin USplineComponent::SetRotationAtSplinePoint(). Origin function has a bug in case of ESplineCoordinateSpace::World */
	void SetRotationAtSplinePoint_Fixed(int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline=true);

	static void SetRotationAtSplinePoint_Fixed(USplineComponent* Spline, int32 PointIndex, const FRotator& InRotation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUpdateSpline /*= true*/);

	void ValidateConnections();
	
	/** Marks this component as needing procedural regeneration and broadcasts
	 *  FMetaRoadDelegates::OnRoadComponentDirtyDelegate. */
	virtual void MarkRoadStateDirty();

public:
	FRoadPosition GetRoadPosition(int SectionIndex, int LaneIndex, double Alpha, double SOffset, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FRoadPosition GetRoadPosition(double SOffset, double ROffset, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Total lateral offset (from the reference line, incl. EvalROffset) of an attribute key's anchor.
	 *  ZeroLaneIndex -> Value.GetCenterLaneROffset(section edge offsets); other lanes ->
	 *  EvalLaneROffset(LaneIndex, SOffset, Value.GetKeyAlpha()). Single source for the centre-lane
	 *  vs normal-lane anchor split shared by DrawKey / visualizers / Polygon. */
	double EvalAttributeAnchorROffset(int SectionIndex, int LaneIndex, double SOffset, const FRoadLaneAttributeValue& Value) const;

	/** Full road position (Location + Quat) of an attribute key's anchor. */
	FRoadPosition EvalAttributeAnchorPosition(int SectionIndex, int LaneIndex, double SOffset, const FRoadLaneAttributeValue& Value, ESplineCoordinateSpace::Type CoordinateSpace) const;

	/** Tests a line segment against all lane polygons of this spline.
	 *  Populates OutHit with the closest hit point, section index, and lane index.
	 *  Returns true if any lane was hit. */
	bool LineTrace(const FVector& Start, const FVector& End, FRoadHitResult& OutHit) const;

	/**
	 * Ensures all lane connection Guids are current and mirrored into serializable form:
	 * - Calls ULaneConnection::GetAndUpdateGuid() on every LC (frame-deduped regeneration).
	 * - Sets URoadConnection::LaneConnectionGuid = connected LC's Guid (or {} if unconnected).
	 * Must be called before any T3D or binary duplication so that LaneConnectionGuid values
	 * are valid when serialized. Called by PreDuplicate() (binary dup) and by
	 * UMetaRoadSubsystem::OnDuplicateActorsBegin() (actor T3D duplicate/paste).
	 */
	void RefreshConnectionGuids();

	using FGetRoadPositionFunc = TFunction<FRoadPosition(double SOffset, ESplineCoordinateSpace::Type CoordinateSpace)>;
	bool ConvertSplineToPolyline(
		const FGetRoadPositionFunc& GetRoadPositionFunc, 
		ESplineCoordinateSpace::Type CoordinateSpace, 
		double MaxSquareDistanceFromSpline, double MinSegmentLength,
		double S0, double S1, 
		const TArray<double>& AdditionalSegments,
		bool bAllowWrappingIfClosed,
		TArray<FRoadPosition>& OutPoints) const;

public:
	virtual void UpdateSpline() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool ShouldRenderSelected() const override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None) override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void PostEditImport() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	virtual void PushSelectionToProxy() override;


protected:
	bool AdjustArcSegment(int PointIndex, EComputeArcMode Mode);
	void AdjustLineSegment(int SegmentIndex);
	bool CheckArc(int PointIndex) const;
	void FixUpSegments();


	int GetNextPoint(int iSegment) const;
	int GetPrevPoint(int iSegment) const;

	virtual void MagicUpdateTransformInner(TFunction<bool(const URoadSplineComponent*)> Filter);

	bool DivideSplineIntoPolylineRecursiveWithDistancesHelper2(const FGetRoadPositionFunc& GetRoadPositionFunc, double S0, double S1, ESplineCoordinateSpace::Type CoordinateSpace, double MaxSquareDistanceFromSpline, double MinSegmentLength, TArray<FRoadPosition>& OutPoints) const;

protected:
	UPROPERTY();
	URoadSplineMetadata* SplineMetadata = nullptr;

	int SelectedSectionIndex = INDEX_NONE;
	int SelectedLaneSectionIndex = 0;

	/** All editor-selected lanes as (SectionIndex, LaneIndex). Transient; mirrors the editor selection
	 *  state for multi-lane highlighting in the scene proxy. The primary lane above is always included
	 *  implicitly (see IsLaneSelected). */
	TArray<TTuple<int, int>> SelectedLanes;

	/** Transient editor highlight for the looped fill area (see SetLoopSelected/IsLoopSelected). */
	bool bLoopSelected = false;

	/**
	 * Set by PostEditImport() (T3D paste) and PostDuplicate() (binary duplication).
	 * Consumed and cleared by OnRegister(): triggers OnSplinePostEditImport() on the subsystem,
	 * deferring EndCopySplineTransaction() to the next Tick so that all pasted splines are
	 * registered before connection re-linking is attempted.
	 */
	bool bWasImported = false;

	void RestoreConnectionGuids();

public:
	// Flat snapshot of LC Guid values (section→Left→pred/succ, Right→pred/succ order).
	// Captured by CaptureConnectionGuids() before binary dup; applied and cleared by
	// RestoreConnectionGuids() after UpdateRoadLayout() creates fresh ULaneConnection objects.
	UPROPERTY()
	TArray<FGuid> ConnectionGuidSnapshot;

	void CaptureConnectionGuids();

protected:
};


/** 
 * Used to store spline data during RerunConstructionScripts 
 */
USTRUCT()
struct METAROAD_API FDriveSplineInstanceData : public FSplineInstanceData
{
	GENERATED_BODY()
public:
	FDriveSplineInstanceData()
	{}
	explicit FDriveSplineInstanceData(const URoadSplineComponent* SourceComponent)
		: FSplineInstanceData(SourceComponent)
	{}
	virtual ~FDriveSplineInstanceData() = default;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<URoadSplineComponent>(Component)->ApplyComponentInstanceData(this, (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript));
	}

	UPROPERTY()
	TArray<ERoadSplinePointTypeOverride> PointTypes;
};


