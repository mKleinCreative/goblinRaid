/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/EngineTypes.h"
#include "MetaRoadEditorModule.h"
#include "MetaRoadTypes.h"
#include "Assets/MetaRoadPresetBase_DEPRECATED.h"
#include "Assets/RoadProfile.h"
#include "Assets/RoadLaneAttributeGenerate.h"
#include "MetaRoadPreset_DEPRECATED.generated.h"

class UActorComponent;
class AActor;

/**
 * UCustomSplineBuilder
 */
UCLASS(abstract, BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class METAROADEDITOR_API UCustomSplineBuilder : public UObject
{
	GENERATED_BODY()

public:
	virtual void GenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  URoadLaneAttributeDescriptor* Profile, AActor* TargetActor, bool bIsPreview) const
	{
		ReceiveGenerateAsset(SplineMeshParams, Profile, TargetActor, bIsPreview);
	}

	UFUNCTION(BlueprintImplementableEvent, Category = "CustomSplineBuilder", meta = (DisplayName = "Generate Asset"))
	void ReceiveGenerateAsset(const FReferenceSplineMeshParams& SplineMeshParams, const  URoadLaneAttributeDescriptor* Profile, AActor* TargetActor, bool bIsPreview) const;
};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntry
{
	GENERATED_BODY()

	FRoadLaneAttributeEntry() = default;
	virtual ~FRoadLaneAttributeEntry() = default;

	FRoadLaneAttributeEntry(const TInstancedStruct<FRoadLaneAttributeValue>& AttributeValueTemplate, FText LabelOverride, FText ToolTip, const FName& IconStyleName)
		: AttributeValueTemplate(AttributeValueTemplate)
		, LabelOverride(LabelOverride)
		, ToolTip(ToolTip)
		, IconStyleName(IconStyleName)

	{
	}

	// Struct child of RoadLaneAttributeValue
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	TInstancedStruct<FRoadLaneAttributeValue> AttributeValueTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FText LabelOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FText ToolTip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FName IconStyleName = "RoadEditor.RoadLaneBuildMode";


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	FName StyleName = "MetaRoadEditor"; //FMetaRoadEditorStyle::Get().GetStyleSetName();

	inline FSlateIcon GetIcon() const
	{
		return  FSlateIcon(StyleName, IconStyleName);
	}

};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryRefSpline : public FRoadLaneAttributeEntry
{
	GENERATED_BODY()

	FRoadLaneAttributeEntryRefSpline()
	{
		//AttributeValueTemplate.InitializeAs<FRoadLaneGeneration>();
	}

	// Desired length of each mesh segment placed on spline [cm] 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry, meta = (UIMin = 1.0, ClampMin = 1.0))
	double LengthOfSegment = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	bool bAlignWorldUpVector = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry)
	bool bReversSplineDirection = false;

};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntrySplineMesh : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TObjectPtr<class UStaticMesh> StaticMesh;

};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryComponentTemplate : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TSubclassOf<class USceneComponent> ComponentTemplate;

	// Determines in which part (by SOffset) of the segment the components should be placed: 0.0 - start, 1.0 - end, 0.5 - middle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeEntry, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0))
	double ComponentToSegmentAlign = 0.0;

};

USTRUCT(BlueprintType)
struct FRoadLaneAttributeEntryCustomBuilder : public FRoadLaneAttributeEntryRefSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneAttribute)
	TSubclassOf<class UCustomSplineBuilder> CustomBuilder;

};

/**
 * FRoadLaneSectionProfile
 */
USTRUCT()
struct METAROADEDITOR_API FRoadLaneSectionProfile
{
	GENERATED_USTRUCT_BODY();

	// Profile name for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString ProfileName;

	// Profile category for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString Category;

	// Tooltip for UI only
	UPROPERTY(EditAnywhere, Category = UI)
	FString Tooltip;

	UPROPERTY(EditAnywhere, Category = LaneSection)
	TArray<FRoadLaneProfile> Left;

	UPROPERTY(EditAnywhere, Category = LaneSection)
	TArray<FRoadLaneProfile> Right;

	UPROPERTY(EditAnywhere, NoClear, Category = RoadLane, Export)
	TSet<FRoadLaneAttributeProfile> CenterAttributes;

	inline FString GetFullName() const { return Category + TEXT(".") + ProfileName; }
};

/**
 * UMetaRoadPreset
 */
UCLASS()
class METAROADEDITOR_API UMetaRoadPreset : public UMetaRoadPresetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, NoClear, Category = Preset)
	TMap<FName, TInstancedStruct<FRoadLaneAttributeEntry>> RoadAttributeEntries;

	UPROPERTY(EditAnywhere, NoClear, Category = Preset)
	TArray<FRoadLaneSectionProfile> RoadLanesProfiles;
};