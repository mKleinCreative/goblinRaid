/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveFloat.h"
#include "Components/SplineComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Animation/AttributeCurve.h"
#include "RoadLaneAttribute.h"
#include "ZoneGraphTypes.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "MetaRoadTypes.generated.h"


namespace MetaRoad
{
	/** Zero index of RoadLane */
	constexpr int ZeroLaneIndex = 0;

	constexpr double DefaultRoadLaneWidth = 375.0;
	
	/** Trims Curve to [Time0, Time1]; if bFitBorders, inserts keys at the boundary times preserving values. */
	METAROAD_API void TrimCurveInRang(FRichCurve& Curve, double Time0, double Time1, bool bFitBorders);
	/** Returns a quaternion that represents the opposite lateral direction (negates the R-axis rotation). */
	METAROAD_API FQuat InvertRotation(const FQuat& Quat);

	FORCEINLINE int LaneIndex2SeqIndex(int LeftNum, int LaneIndex)
	{
		check(LaneIndex != MetaRoad::ZeroLaneIndex);
		return LaneIndex + LeftNum + (LaneIndex > 0 ? -1 : 0);
	}

	FORCEINLINE int SeqIndex2LaneIndex(int LeftNum, int SeqIndex)
	{
		check(SeqIndex >= MetaRoad::ZeroLaneIndex);
		return SeqIndex - LeftNum + (SeqIndex >= LeftNum ? +1 : 0);
	}

};

class URoadSplineComponent;
class URoadConnection;
class ULaneConnection;
class UMaterialInterface;
class FPrimitiveSceneProxy;
struct FRoadLayout;

struct FMetaRoadDelegates
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRegisterRoadSplineChanged, URoadSplineComponent*);

	/** Fired after a URoadSplineComponent is registered with MetaRoadSubsystem. */
	static METAROAD_API FOnRegisterRoadSplineChanged OnRoadSplineRegistredDelegate;
	/** Fired after a URoadSplineComponent is unregistered from MetaRoadSubsystem. */
	static METAROAD_API FOnRegisterRoadSplineChanged OnRoadSplineUnregistredDelegate;

	/** Fired when road component data changes and procedural mesh generation needs a rebuild. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoadComponentDirty, UActorComponent*);
	static METAROAD_API FOnRoadComponentDirty OnRoadComponentDirtyDelegate;

	/** Fired from URoadSplineComponent::UpdateLandscape(). Handled by the editor module (which owns
	 *  the editor-only UMetaRoadLandscapeSubsystem), keeping the runtime module decoupled from
	 *  landscape editing. See UMetaRoadLandscapeSubsystem::HandleSplineLandscapeUpdateRequest. */
	DECLARE_DELEGATE_OneParam(FRequestLandscapeUpdate, URoadSplineComponent*);
	static METAROAD_API FRequestLandscapeUpdate RequestLandscapeUpdateDelegate;

	/** Factory for the editor-only schematic scene proxy of a road spline. Bound by the editor module
	 *  (which owns FRoadSplineSceneProxy / FLaneProxy and the RHI schematic code), keeping the runtime
	 *  module free of the editor render types. Returns nullptr when unbound (e.g. packaged game). */
	DECLARE_DELEGATE_RetVal_OneParam(FPrimitiveSceneProxy*, FCreateRoadSplineSceneProxy, URoadSplineComponent*);
	static METAROAD_API FCreateRoadSplineSceneProxy CreateRoadSplineSceneProxyDelegate;

	/** Reports the materials used by the schematic scene proxy (for GetUsedMaterials material relevance).
	 *  Bound by the editor module; the preview materials now live on UMetaRoadEditorSettings. */
	DECLARE_DELEGATE_TwoParams(FGetRoadSplineUsedMaterials, const URoadSplineComponent*, TArray<UMaterialInterface*>&);
	static METAROAD_API FGetRoadSplineUsedMaterials GetRoadSplineUsedMaterialsDelegate;
};



/**
 * FRoadZoneTypeDetails corresponds to one of the FRoadZoneType.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadZoneTypeDetails
{
	GENERATED_USTRUCT_BODY()

	FRoadZoneTypeDetails();

#if WITH_EDITORONLY_DATA
	FRoadZoneTypeDetails(
		const FLinearColor& InEditorColor, 
		const TSoftObjectPtr<UMaterialInterface>& InEditorMaterial, 
		const TObjectPtr<UMaterialInterface>& InMeshMaterial,
		const TObjectPtr<UMaterialInterface>& InDecalMateria, 
		int InMaterialPriority,
		bool bInIsDecal,
		const FString& InDescription)
		: EditorColor(InEditorColor)
		, EditorMaterial(InEditorMaterial)
		, MeshMaterial(InMeshMaterial)
		, DecalMaterial(InDecalMateria)
		, MaterialPriority(InMaterialPriority)
		, bIsDecal(bInIsDecal)
		, Description(InDescription)
	{
	}
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	FLinearColor EditorColor = FLinearColor::White;

	/** This material is used only in editor mode for pre-visualization. */
	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	TSoftObjectPtr<UMaterialInterface> EditorMaterial;

	/** Default material for mesh generation in "Build Mesh" mode. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = RoadZoneType)
	TObjectPtr<UMaterialInterface> MeshMaterial = nullptr;

	/** Default decal material for mesh generation in "Build Mesh" mode. If the value is empty, the decal will not be built for this type of lane.*/
	UPROPERTY(EditAnywhere, NonTransactional, Category = RoadZoneType)
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	/** Default material priority for mesh generation in "Build Mesh" mode. Used when road lane intersect. 
	  * The triangle will be assigned the material with the highest priority. */
	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	int MaterialPriority = 0;

	/** When true, this zone does not contribute to road mesh height — it is projected onto
	  * the existing mesh surface like a flat overlay. Intended for asphalt markings (arrows,
	  * stop lines). Unrelated to Unreal's UDecalComponent. */
	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	bool bIsDecal = false;

	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	FString Description = {};
#endif

#if WITH_EDITOR
	UMaterialInterface* GetEditorMaterial(bool bUpdateColor = true) const;
	void UpdateEditorColor() const;
	void ResetEditorMaterial();
#endif
private:

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	mutable TObjectPtr<UMaterialInstanceDynamic> EditorMaterialDynamic = nullptr;
#endif
};

/**
 * Type of road zone (or surface). 
 * Any road surface consists of a set of zones. For example, driveable road lane, sidewolk, mark, tram etc.
 * As a rule, zones are either arbitrary polygons or road lanes.
 * Each FRoadZoneType corresponds to one FRoadZoneTypeDetails.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadZoneType
{
	GENERATED_USTRUCT_BODY()

	FRoadZoneType()
	{
	}

	FRoadZoneType(const FName InName)
		: TypeName(InName)
	{
	}

	FRoadZoneType(const TCHAR* InName)
		: TypeName(FName(InName))
	{
	}

	FRoadZoneType(const ANSICHAR* InName)
		: TypeName(FName(InName))
	{
	}

	bool IsValid() const
	{
		return TypeName.IsValid() && GetDetails();
	}

	FName GetName() const { return TypeName; }

	friend uint32 GetTypeHash(const FRoadZoneType& RoadZoneType)
	{
		return GetTypeHash(RoadZoneType.TypeName);
	}

	bool operator == (const FRoadZoneType& Other) const
	{
		return TypeName == Other.TypeName;
	}

	const FRoadZoneTypeDetails* GetDetails() const;

private:
	UPROPERTY(EditAnywhere, Category = RoadZoneType)
	FName TypeName{};
};


/**
 * Presets of default RoadZoneTypes. New types can be added in UMetaRoadSettings::RoadZoneTypes
 */
struct ERoadZoneTypes
{
	static METAROAD_API const FRoadZoneType Driving; /**  Describes a "normal" drivable road that is not one of the other types. */
	static METAROAD_API const FRoadZoneType Sidewalk; /** Describes a lane on which pedestrians can walk. */
	static METAROAD_API const FRoadZoneType Shoulder; /** Describes a soft border at the edge of the road.*/
	static METAROAD_API const FRoadZoneType Border; /** Describes a hard border at the edge of the road. It has the same height as the drivable lane. */
	static METAROAD_API const FRoadZoneType Stop; /** Hard shoulder on motorways for emergency stops.*/
	static METAROAD_API const FRoadZoneType Biking; /**  Describes a lane that is reserved for cyclists.*/
	static METAROAD_API const FRoadZoneType Restricted; /** Describes a lane on which cars should not drive. The lane has the same height as drivable lanes. Typically, the lane is separated with lines and often contains dotted lines as well.*/
	static METAROAD_API const FRoadZoneType Parking; /** Describes a lane with parking spaces.*/
	static METAROAD_API const FRoadZoneType Median; /** Describes a lane that sits between driving lanes that lead in opposite directions. It is typically used to separate traffic in towns on large roads. */
	static METAROAD_API const FRoadZoneType Marking; /** Marking zones on asphalt created by a separate Looped Road Spine. Typically used for arrows. */
	static METAROAD_API const FRoadZoneType Tram;
	static METAROAD_API const FRoadZoneType Rail;
	static METAROAD_API const FRoadZoneType Bus;
};


/**
 * Used to change lane direction defined in ERoadDirection
 */
UENUM(BlueprintType)
enum class ERoadLaneDirection : uint8
{
	/** Does not change the lane direction defined in ERoadDirection */
	Default,

	/** Inverts the direction of one lane defined in ERoadDirection */
	Invert,
};

/**
 * Specifies which sides of a FRoadLaneSection contain active lane data.
 *
 * Left and right sides can have section boundaries at different SOffset positions
 * (asymmetric layout). When Side is Left or Right, FRoadLayout::UpdateLayout()
 * discards the unused side's lane array. To find the physical section that actually
 * holds lane data for a given side at an arbitrary SOffset, use
 * FRoadLayout::FindSideSection().
 */
UENUM(BlueprintType)
enum class ERoadLaneSectionSide : uint8
{
	Both,   // Both Left[] and Right[] lane arrays are defined
	Left,   // Only Left[] is defined; UpdateLayout() clears Right[]
	Right   // Only Right[] is defined; UpdateLayout() clears Left[]
};

/**
 * ERoadDirection defines the common lanes direction in road spline (URoadSplineComponent).
 */
UENUM(BlueprintType)
enum class ERoadDirection : uint8
{
	/** Left-hand traffic, left lanes (FRoadLaneSection::Left) are aligned with the URoadSplineComponent direction,
	  * while right lanes (FRoadLaneSection::Right) are reverse URoadSplineComponent direction. */
	LeftHand,

	/** Right-hand traffic, right lanes (FRoadLaneSection::Right) are aligned with the URoadSplineComponent directin, 
 	  * while left lanes (FRoadLaneSection::Left) are reverse URoadSplineComponent direction. */
	RightHand
};


/**
 * FRoadZone is a base class for storing road zone (surface) data. 
 * While FRoadZoneType + FRoadZoneTypeDetails provide global information about road zones, FRoadZone is data that is assigned to a specific road zone (FRoadLane or Polygon).
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadZone
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRoadZone() {}

	/** Lane surface type. It is primarily used to determine the lane material. However, it can also be used for any other purpose. */
	UPROPERTY(EditAnywhere, Category = RoadLane)
	FRoadZoneType ZoneType{};

	/** Whether to override material from ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (InlineEditConditionToggle))
	bool bOverrideMaterial = false;

	/** Whether to override decal material from ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (InlineEditConditionToggle))
	bool bOverrideDecal = false;

	/** Whether to override material priority from ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (InlineEditConditionToggle))
	bool bOverridePriority = false;

	/** Override default material from the ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (EditCondition = "bOverrideMaterial"))
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	/** Override default decal material from the ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (EditCondition = "bOverrideDecal"))
	TObjectPtr<UMaterialInterface> OverrideDecal = nullptr;

	/** Override default material priority from the ZoneType */
	UPROPERTY(EditAnywhere, Category = RoadLane, meta = (EditCondition = "bOverridePriority"))
	int OverridePriority = 0;
};


/**
 * FRoadZoneDriving describes the road zone (surface) on which vehicles may potentially move. 
 * For example: a regular road, a shoulder, a bike lane, a tram lane, etc.
 * During procedural mesh generation, all FRoadZoneDriving zones are treated as a single mesh, but with different materialIDs.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadZoneDriving : public FRoadZone
{
	GENERATED_USTRUCT_BODY()

public:
	FRoadZoneDriving()
	{
		ZoneType = ERoadZoneTypes::Driving;
	}
	
	// Invert U coordinate for UV0 for procedure generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInvertUV0 = false;
};

/**
 * FRoadZoneSidewalk describes the road zone (surface) on which only pedestrians can move.
 * During procedural mesh generation, all FRoadZoneSidewalk zones are treated as a single mesh, but with different materialIDs.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadZoneSidewalk : public FRoadZone
{
	GENERATED_USTRUCT_BODY()

public:
	FRoadZoneSidewalk();

	/** Default height, can be overrided by the "Sidewalk Height Attribute". See URoadLaneAttributeSidewalkHeightDescriptor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	double DefaultHeight = 15;

	//TODO. Not supported yet
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bIsSoftBorder = false;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	//double CurbWidth = 15;

	/** Whether to build the inside curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bInsideCurb = true;

	/** Whether to build the outside curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bOutsideCurb = true;

	/** Whether to build the begining  cap curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bBeginCurb = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bBeginCurb", EditConditionHides))
	FRuntimeFloatCurve BeginCapCurve;

	/** Whether to build the ending cap curb (in the direction of the spline) when using procedural generation for this road lane  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bEndCurb = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane, meta = (EditCondition = "bEndCurb", EditConditionHides))
	FRuntimeFloatCurve EndCapCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	TSoftObjectPtr<class URoadCurbProfile> CurbProfile;

	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (InlineEditConditionToggle))
	bool bOverrideCurbMaterial = false;

	/** Override default material from profile */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (EditCondition = "bOverrideCurbMaterial"))
	TObjectPtr<UMaterialInterface> OverrideCurbMaterial = nullptr;
};


/**
 * Determines road lanes within a FRoadLaneSection.
 * Lanes are attached to the road reference spline (URoadSplineComponent) and are defined from inside to outside.
 *  Each lane has an index within the FRoadLaneSection:
 *    - Indexes > 0 are to the right of the reference spline. 
 *    - Indexes < 0 are to the left of the reference spline.
 *    - Indexes == 0 (or MetaRoad::ZeroLaneIndex) are invalid but denote the centerline of the FRoadLaneSection.
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#road-lanes 
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLane
{
	GENERATED_USTRUCT_BODY()

	friend FRoadLayout;

	FRoadLane();

	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = RoadLane, Export)
	TInstancedStruct<FRoadZone> RoadZone;

	/** 
	 * Attributes are arbitrary metadata that can be assigned along the road lane.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = RoadLane)
	TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute> Attributes;

	/** Lane width curve */
	UPROPERTY(EditAnywhere, Category = RoadLane)
	FRichCurve Width;

	/** Direction of the road lane. See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	ERoadLaneDirection Direction = ERoadLaneDirection::Default;

	/** Skip procedural generation for this lane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	bool bSkipProceduralGeneration = false;

	/** Used for any user purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	TMap<FString, FString> StringTags;

	/** Used for any user purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	TMap<FString, double> FloatTags;

	/** Used for any user purpose and for generate zone graph */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	FZoneGraphTagMask ZoneTags;

	/**  Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadLane)
	FString Label{};

	/** 
	 * Used to organize link between the FRoadLane and others URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Export, Category = RoadLane)
	TObjectPtr< ULaneConnection> PredecessorConnection;

	/** 
	 * Used to organize link between the FRoadLane and others URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Export, Category = RoadLane)
	TObjectPtr< ULaneConnection> SuccessorConnection;

	/** End SOffset position (from the beginning of the spline) of this lane. This value set from URoadSplineComponent::UpdateLaneSectionBounds() */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RoadLane)
	double SOffsetEnd_Cashed = 0;

private:
	/** Set from URoadSplineComponent::UpdateRoadLayout() */
	TWeakPtr<FRoadLayout*> OwnedRoadLayout {};

	/** Set from URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int StartSectionIndex = INDEX_NONE;

	/** Set from URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int EndSectionIndex = INDEX_NONE; 

	/** Set from URoadSplineComponent::UpdateRoadLayout() */
	UPROPERTY(VisibleAnywhere, Category = RoadLane, Transient)
	int LaneIndex = MetaRoad::ZeroLaneIndex;


public:

	FRoadZoneType GetType() const;

	bool IsLaneValid() const;
	bool IsForwardLane() const;

	int GetStartSectionIndex() const { return StartSectionIndex; }
	int GetEndSectionIndex() const { return EndSectionIndex; }
	int GetLaneIndex() const { return LaneIndex; }

	const FRoadLaneSection& GetStartSection() const;
	FRoadLaneSection& GetStartSection();

	const FRoadLaneSection& GetEndSection() const;
	FRoadLaneSection& GetEndSection();

	double GetStartOffset() const;
	double GetEndOffset() const { return SOffsetEnd_Cashed; }

	/** Clips Width curve and Attributes to the section's SOffset bounds.
	 *  bFitWidth — if true, inserts boundary keys at the section edges before clipping. */
	void Trim(bool bFitWidth);
};


/**
 * Container for road lanes (FRoadLane) starting at a fixed SOffset along URoadSplineComponent.
 * Sections are stored in FRoadLayout::Sections[] in ascending SOffset order.
 *
 * Asymmetric layout: when Side == Left or Side == Right, only that side's lane
 * array is valid. This allows the left and right sides to have section boundaries
 * at different SOffset positions. For example, a Right-only section at S=500
 * introduces new right-side lanes there while the left side continues using lanes
 * from the previous Both/Left section. To get the actual lane data for a given
 * side at an arbitrary SOffset, use FRoadLayout::FindSideSection() or
 * FRoadLayout::GetLeftRighLanes() — do NOT assume Left[]/Right[] are populated
 * in every section.
 *
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-sections
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneSection
{
	GENERATED_USTRUCT_BODY();

	friend FRoadLayout;

	FRoadLaneSection();
	~FRoadLaneSection();

	FRoadLaneSection(FRoadLaneSection&& Other) noexcept;
	FRoadLaneSection& operator = (FRoadLaneSection&& Other) noexcept;

	FRoadLaneSection(const FRoadLaneSection& Other) noexcept;
	FRoadLaneSection& operator = (const FRoadLaneSection& Other) noexcept;

	/** Which lane arrays are active in this section.
	 *  Both  — Left[] and Right[] are both populated (normal case).
	 *  Left  — only Left[] is populated; UpdateLayout() discards Right[].
	 *  Right — only Right[] is populated; UpdateLayout() discards Left[].
	 *  Use FRoadLayout::FindSideSection() to resolve asymmetric layouts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	ERoadLaneSectionSide Side = ERoadLaneSectionSide::Both;

	/** 
	 * Road lanes on the left side of the URoadSplineComponent. 
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-groups */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Export, Category = LaneSection)
	TArray<FRoadLane> Left;

	/** 
	 * Road lanes on the right side of the URoadSplineComponent.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-groups */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Export, Category = LaneSection)
	TArray<FRoadLane> Right;

	/** 
	 * Center lane attributes.
	 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = LaneSection)
	TMap<TSoftClassPtr<URoadLaneAttributeDescriptor>, FRoadLaneAttribute> Attributes;

	/** SOffset position (from the beginning of the spline) of this section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	double SOffset = 0;

	/** End SOffset position (from the beginning of the spline) of this section. This value automatically calucated in the UpdateLaneSectionBounds() */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LaneSection)
	double SOffsetEnd_Cashed = 0;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	FString Label{};

	/** Skip landscape deformation for this entire section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneSection)
	bool bSkipLandscape = false;

	bool IsSectionValid() const;
	const FRoadLane& GetLaneByIndex(int LaneIndex) const;
	FRoadLane& GetLaneByIndex(int LaneIndex);
	bool CheckLaneIndex(int LaneIndex) const;
	const FRoadLaneAttribute* FindAttribute(int LaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute) const;
	FRoadLaneAttribute* FindAttribute(int LaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute);

	/** Fit Width and Attributes to far all lane of the this section bounds */
	void Trim(bool bFitWidth);

	/**
	 * Returns the R-axis offset of the outer edge of lane LaneIndex at position SOffset.
	 * @param LaneIndex  >0 = right lane, <0 = left lane (MetaRoad::ZeroLaneIndex is invalid)
	 * @param SOffset    Arc-length distance from spline start
	 * @param Alpha      Edge selector: 0.0 = inner edge (nearest center), 1.0 = outer edge (default)
	 */
	double EvalLaneROffset(int LaneIndex, double SOffset, double Alpha = 1.0) const;

	/**
	 * Outer-edge R-offsets of this section's leftmost and rightmost lanes at position SOffset.
	 * Uses this section's own Left/Right arrays (NOT the asymmetric GetLeftRighLanes resolution),
	 * returning 0.0 for an empty side. Result: { LeftR (<= 0), RightR (>= 0) }.
	 * Shared by the centre-lane (ZeroLaneIndex) anchor logic in the editor and the mesh pipeline.
	 */
	TPair<double, double> GetCenterLaneEdgeROffsets(double InSOffset) const;

	inline bool operator < (const FRoadLaneSection& Other) const { return SOffset < Other.SOffset; }

	inline bool IsSectionEmpty() const { return Left.IsEmpty() && Right.IsEmpty(); }

private:
	/** Set from FRoadLayout::UpdateLayout() */
	TWeakPtr<FRoadLayout*> OwnedRoadLayout{}; 

	/** Set from FRoadLayout::UpdateLayout() */
	UPROPERTY(VisibleAnywhere, Category = LaneSection, Transient)
	int SectionIndex = INDEX_NONE; 
};

/**
 * FRoadLayout determine the scope of all road lanes for URoadSplineComponent.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLayout
{
	GENERATED_USTRUCT_BODY();

	FRoadLayout();

	FRoadLayout(FRoadLayout&& Other) noexcept;
	FRoadLayout& operator = (FRoadLayout&& Other) noexcept;

	FRoadLayout(const FRoadLayout& Other) noexcept;
	FRoadLayout& operator = (const FRoadLayout& Other) noexcept;

	/** Sections sorted in ascending SOffset order. Left and right sides may have
	 *  section boundaries at different positions (asymmetric layout, Side != Both).
	 *  Always call UpdateLayout() after any structural change to this array. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RoadLayout)
	TArray<FRoadLaneSection> Sections;

	/** Right offset of the road layout relative to URoadSplineComponent.
	  * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-offset */
	UPROPERTY(EditAnywhere, Category = RoadLayout)
	FRichCurve ROffset;

	/** Common direction of the road.  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RoadLayout)
	ERoadDirection Direction = ERoadDirection::RightHand;

	/** Supported only for closed splines (URoadSplineComponent::bClosedLoop == true). 
	  * Fill the outline formed by the closed URoadSplineComponent with this RoadZone.
	  * Mainly used for procedural generation to draw the refuge islands or pedestrian crossings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LoopedRoadZone)
	TInstancedStruct<FRoadZone> LoopedRoadZone;

	/** UV textures rotation for the LoopedRoadZone. Used for procedure generation only. [deg]*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LoopedRoadZone)
	double LoopedRoadZoneTexAngle = 0;

	/** UV textures scale for the LoopedRoadZone. Used for procedure generation only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LoopedRoadZone)
	double LoopedRoadZoneTexScale = 1.0;
	
	/** Used for debug or inner purpose */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = LoopedRoadZone)
	bool bDontCreateLaneConnections = false;

	/** Rebuilds internal back-pointers (OwnedRoadLayout, SectionIndex, LaneIndex) for all
	 *  sections and lanes. Must be called after any structural change to Sections[]. */
	void UpdateLayout(URoadSplineComponent* OwnedRoadSpline);

	/** Recomputes SOffsetEnd_Cashed for all sections and lanes based on SplineLength.
	 *  Called automatically by UpdateLayout(); call directly when only spline length changes. */
	void UpdateBounds(double SplineLength);

	/** Removes sections and lanes that fall outside [0, SplineLength] and clips
	 *  Width/Attribute curves to their section bounds. Call after UpdateLayout(). */
	void TrimSections(double SplineLength, double Tolerance, URoadSplineComponent* OwnedRoadSpline);

	double EvalROffset(double S) const;

	/** Walks backwards from SectionIndex to find the nearest section that has active
	 *  lane data for the requested Side (i.e. its Side equals Both or the requested side).
	 *
	 *  Required for asymmetric layouts: a Right-only section has no Left data, so the
	 *  effective left lanes come from an earlier Both/Left section. Returns 0 if no
	 *  matching section is found before the beginning of the array.
	 *  @param Side  Must be Left or Right — not Both. */
	int FindSideSection(int SectionIndex, ERoadLaneSectionSide Side) const;

	int FindSectionAtSOffset(double S) const;

	/** Returns {leftSection, rightSection} — the sections that actually contain Left[]
	 *  and Right[] lane data for SectionIndex, resolving asymmetric layouts via
	 *  FindSideSection(). The two references may point to different, earlier sections
	 *  when the sides have independent section boundaries. */
	TPair<FRoadLaneSection&, FRoadLaneSection&> GetLeftRighSections(int SectionIndex);
	TPair<const FRoadLaneSection&, const FRoadLaneSection&> GetLeftRighSections(int SectionIndex) const;

	/** Returns {leftSection.Left, rightSection.Right} — the lane arrays for both sides
	 *  at SectionIndex. Resolves asymmetric layouts via GetLeftRighSections().
	 *  Prefer this over accessing Left[]/Right[] directly. */
	TPair<TArray<FRoadLane>&, TArray<FRoadLane>&> GetLeftRighLanes(int SectionIndex);
	TPair<const TArray<FRoadLane>&, const TArray<FRoadLane>&> GetLeftRighLanes(int SectionIndex) const;

private:
	TSharedPtr<FRoadLayout*> ThisShared;
};

/**
 * Spline-level connection endpoint. One `URoadConnection` sits at the start and one at the end of
 * every `URoadSplineComponent`. It represents the whole-road side of an intersection link and holds
 * a soft pointer to exactly one `ULaneConnection` on the other road ("outer" = the far side).
 *
 * Relationship: URoadConnection (1) ←→ (1) ULaneConnection
 * Direction: forward transform co-directed with the owning URoadSplineComponent.
 *
 * Copy/paste: before any T3D or binary duplication, `RefreshConnectionGuids()` sets `LaneConnectionGuid` to the
 * current `Guid` of the connected `ULaneConnection`. After paste, `UMetaRoadSubsystem::EndCopySplineTransaction`
 * uses this value to re-link the copy to the matching `ULaneConnection` copy.
 *
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions
 */
UCLASS()
class METAROAD_API URoadConnection: public UObject
{
	GENERATED_UCLASS_BODY()

	friend URoadSplineComponent;

public:
	/** The ULaneConnection on the far road that this endpoint is linked to. Null when unconnected. */
	UPROPERTY(VisibleAnywhere, Category = RoadConnection)
	TSoftObjectPtr<ULaneConnection> OuterLaneConnection;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadConnection)
	FString Label{};

	/**
	 * Snapshot of the connected ULaneConnection::Guid taken by URoadSplineComponent::RefreshConnectionGuids()
	 * before T3D/binary duplication. After paste, EndCopySplineTransaction matches this value against
	 * the pasted ULaneConnection::Guid values to re-establish the connection in the copy.
	 * Valid only transiently around a copy/paste operation; not meaningful at runtime.
	 */
	UPROPERTY()
	FGuid LaneConnectionGuid{};

public:
	URoadSplineComponent* GetOwnedRoadSpline() const;
	URoadSplineComponent* GetOwnedRoadSplineChecked() const;
	virtual bool IsConnectionValid() const;

	virtual bool ConnectTo(ULaneConnection* OuterLaneConnection);
	virtual bool CanConnectTo(const ULaneConnection* OuterLaneConnection, bool bLog=false) const;
	virtual void Disconnect();
	virtual bool IsConnected() const;
	virtual bool IsConnectionFree() const;
	virtual ULaneConnection* GetOuterConnection() const { return OuterLaneConnection.Get(); }

	virtual void InitConnection();

	virtual bool CanTransform() const;
	virtual const FTransform GetTransform(ESplineCoordinateSpace::Type Space) const;
	virtual bool SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space);

	/** Pulls transform from the connected ULaneConnection and applies it to this connection. */
	virtual bool SetTransformFormOuter(bool bUpdateSpline=true);

	/** Pushes this connection's transform to the connected ULaneConnection. */
	virtual bool SetTransformToOuter();

	virtual bool IsSuccessorConnection() const;
	virtual bool IsPredecessorConnection() const;

public:
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	//virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;

	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

	static int32 GlobalTransformMagic;

private:
	/** Used to prevent recursive SetTransform() */
	mutable int32 TransformMagic = 0;
};


/**
 * Lane-level connection endpoint. One `ULaneConnection` sits at the start and one at the end of
 * every `FRoadLane`. It is the "inner" (owned) side of an intersection link: multiple roads can
 * connect their `URoadConnection` endpoints to the same `ULaneConnection` (many-to-one).
 *
 * Relationship: ULaneConnection (1) ←→ (N) URoadConnection  (stored in OuterRoadConnections)
 * Direction: forward transform co-directed with the owning FRoadLane.
 * Back-pointers (OwnedRoadSpline / SectionIndex / LaneIndex) are set by FRoadLayout::UpdateLayout()
 * and must not be written directly.
 *
 * Copy/paste identity: each ULaneConnection carries a stable `Guid`. `GetAndUpdateGuid()` refreshes
 * it once per engine frame (frame-dedup via GuidFrameCounter) so that all callers within the same
 * duplication operation see the same value. EndCopySplineTransaction uses {Guid → ULaneConnection}
 * to match pasted URoadConnection::LaneConnectionGuid values and restore links in the copy.
 *
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#intersections-and-junctions
 */
UCLASS(BlueprintType, Blueprintable)
class METAROAD_API ULaneConnection: public UObject
{
	GENERATED_UCLASS_BODY()

	friend FRoadLayout;
	friend URoadSplineComponent;

public:
	/** All URoadConnections currently linked to this lane endpoint (opposite side of the link). Transient — rebuilt at runtime. */
	UPROPERTY(Transient, DuplicateTransient)
	TSet<TWeakObjectPtr< URoadConnection>>  OuterRoadConnections;

	/** Used for any user purpose (mostly debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneConnection)
	FString Label{};

	UPROPERTY(Transient)
	FTransform CashedTransform{};

public:
	URoadSplineComponent* GetOwnedRoadSpline() const { return OwnedRoadSpline.Get(); }
	URoadSplineComponent* GetOwnedRoadSplineChecked() const { check(OwnedRoadSpline.IsValid()); return OwnedRoadSpline.Get(); }
	int GetSectionIndex() const { return SectionIndex; }
	int GetLaneIndex() const { return LaneIndex; }

	const FRoadLane & GetOwnedRoadLane() const;
	FRoadLane& GetOwnedRoadLane();

	virtual bool IsConnectionValid() const;

	virtual bool IsConnected() const;
	//virtual bool ConnectTo(URoadConnection* RoadConnection);
	virtual void DisconnectAll();

	/**
	 * @param Alpha see FRoadLaneSection::EvalLaneROffset(). By default Alpha=0.0
	 */
	virtual const FTransform EvalTransform(double Alpha, ESplineCoordinateSpace::Type Space) const;

	/** Set transform from this connection to all connected URoadConnection */
	virtual void SetTransformToOuter();

	/** Moves the lane endpoint by adjusting the owning spline's start or end control point.
	 *  Only valid when this connection is at the spline's very start or end; mid-spline
	 *  connections cannot be moved without solving for new tangents (not yet implemented).
	 *  The resulting transform may differ from the requested one due to spline constraints. */
	virtual bool SetTransform(const FTransform& Transform, bool bUpdateSpline, ESplineCoordinateSpace::Type Space);

	virtual bool CanTransform() const;

	bool IsSuccessorConnection() const;
	inline bool IsRoadSuccessorConnection() const;
	inline bool IsRoadPredecessorConnection() const;

	/** Returns Guid, regenerating it with FGuid::NewGuid() at most once per engine frame (GuidFrameCounter dedup). */
	const FGuid& GetAndUpdateGuid();

	const FGuid& GetGuid() const { return Guid; }

public:
	//virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;

	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

private:
	mutable int32 TransformMagic = 0;

	/** Back-pointer to the owning spline. Set by FRoadLayout::UpdateLayout(); do not write directly. */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	TWeakObjectPtr<URoadSplineComponent> OwnedRoadSpline;

	/** Index into URoadSplineComponent::RoadLayout.Sections[]. Set by FRoadLayout::UpdateLayout(); do not write directly. */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	int SectionIndex = INDEX_NONE;

	/** Lane index (signed, 0 = invalid). Set by FRoadLayout::UpdateLayout(); do not write directly. */
	UPROPERTY(VisibleAnywhere, Category = LaneConnection, Transient)
	int LaneIndex = MetaRoad::ZeroLaneIndex;

	/**
	 * Stable identity Guid used to re-link connections after copy/paste and duplication.
	 * Refreshed by GetAndUpdateGuid() (once per frame). NOT marked Transient so that it survives
	 * T3D serialization — the T3D copy carries this value, which EndCopySplineTransaction matches
	 * against URoadConnection::LaneConnectionGuid to restore links in the pasted copy.
	 */
	UPROPERTY()
	FGuid Guid{};

	/** GFrameCounter value at the last GetAndUpdateGuid() call; prevents multiple Guid changes per duplication frame. */
	UPROPERTY()
	uint64 GuidFrameCounter;
};


