/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Engine/EngineTypes.h"
#include "MetaRoadEditorModule.h"
#include "MetaRoadTypes.h"
#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadProfile.generated.h"

/**
 * FRoadLaneAttributeProfile
 */
USTRUCT(BlueprintType)
struct METAROADEDITOR_API FRoadLaneAttributeProfile
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeProfile);
	TSoftClassPtr<URoadLaneAttributeDescriptor> AttributeDesctiptor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeProfile);
	TInstancedStruct<FRoadLaneAttributeValue> AttributeValueTemplate;

	friend uint32 GetTypeHash(const FRoadLaneAttributeProfile& Profile)
	{
		return GetTypeHash(Profile.AttributeDesctiptor);
	}

	inline bool operator==(const FRoadLaneAttributeProfile& Other) const
	{
		return Other.AttributeDesctiptor == AttributeDesctiptor;
	}

	bool IsProfileValid() const
	{
		if (!IsValid(AttributeValueTemplate.GetScriptStruct()))
		{
			return false;
		}
		if (UClass* Class = AttributeDesctiptor.LoadSynchronous())
		{
			return Class->GetDefaultObject<URoadLaneAttributeDescriptor>()->GetAttributeValueTemplate().GetScriptStruct() == AttributeValueTemplate.GetScriptStruct();
		}
		return false;
	}

	bool AssignFrom(const TSoftClassPtr<URoadLaneAttributeDescriptor>& InDescriptor, const FRoadLaneAttribute& InAttribute);
};

/**
 * FRoadLaneProfile
 */
USTRUCT(BlueprintType)
struct METAROADEDITOR_API FRoadLaneProfile
{
	GENERATED_BODY()

	FRoadLaneProfile() = default;
	FRoadLaneProfile(const FRoadLane& FromLane);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneProfile)
	double Width = MetaRoad::DefaultRoadLaneWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Category = LaneProfile, Export)
	TInstancedStruct<FRoadZone> RoadZone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Category = LaneProfile, Export)
	TSet<FRoadLaneAttributeProfile> Attributes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneProfile)
	ERoadLaneDirection Direction = ERoadLaneDirection::Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LaneProfile)
	bool bSkipProceduralGeneration = false;

};


/**
 * Road profile for "Draw Spline Tool"
 */
UCLASS(BlueprintType, Blueprintable)
class METAROADEDITOR_API URoadProfile : public UObject
{
	GENERATED_BODY()

public:

	// Profile name for UI only
	//UPROPERTY(EditAnywhere, Category = UI)
	//FString ProfileName;

	// Profile category for UI only
	//UPROPERTY(EditAnywhere, Category = UI)
	//FString Category;

	// Tooltip for UI only
	//UPROPERTY(EditAnywhere, Category = UI)
	//FString Tooltip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadProfile)
	TArray<FRoadLaneProfile> Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadProfile)
	TArray<FRoadLaneProfile> Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Category = RoadProfile, Export)
	TSet<FRoadLaneAttributeProfile> CenterAttributes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Category = RoadProfile, Export)
	ERoadDirection Direction = ERoadDirection::RightHand;

	UFUNCTION(BlueprintCallable, Category = RoadProfile)
	void AssignToRoadSpline(URoadSplineComponent* TargetSpline) const;

	UFUNCTION(BlueprintCallable, Category = RoadProfile)
	bool AssignFromRoadSection(const FRoadLayout& Layout, int SectionIndex);

	// --- Lane editing (used by the Road Profile asset editor) ---

	/** Resolve a signed lane index to its FRoadLaneProfile: >0 -> Right[idx-1], <0 -> Left[-idx-1], 0 (center) -> nullptr. */
	FRoadLaneProfile* GetLaneByIndex(int32 LaneIndex);
	const FRoadLaneProfile* GetLaneByIndex(int32 LaneIndex) const;

	int32 GetLeftNum() const { return Left.Num(); }
	int32 GetRightNum() const { return Right.Num(); }

	/** Insert a new lane next to the lane at LaneIndex (or next to the center). Returns the new lane's index. */
	int32 AddLane(int32 LaneIndex, bool bOnLeft);

	/** Delete the (non-center) lane at LaneIndex. Returns the new selection (ZeroLaneIndex). */
	int32 DeleteLane(int32 LaneIndex);

	/** Toggle the per-lane Direction (Default<->Invert) of the (non-center) lane at LaneIndex. */
	void ReverseLane(int32 LaneIndex);
};