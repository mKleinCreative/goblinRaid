/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Assets/RoadLaneAttributeDescriptor.h"
#include "RoadLaneAttributeMark.generated.h"

 /**
  * ERoadLaneMark
  * Can be used in gameplay (for example traffic generation). Has no effect on procedural generation.
  */
UENUM(BlueprintType)
enum class ERoadLaneMark : uint8
{
	None,
	Solid,
	Broked,
	DoubleSolid,
	DoubleBroked,
	SolidBroked,
	BrokedSolid,
	Custom
};


USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	/** Can be used in gameplay (for example traffic generation). Has no effect on procedural generation.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	ERoadLaneMark Type = ERoadLaneMark::None;


	//virtual bool MakeCustomMesh(const TArray<struct FRoadPosition>& Line, double S0, double S1, double ZOffset, class FDynamicMesh3& OutDynamicMesh) { return false; }
	//virtual void DrawCustomLine(const TArray<struct FRoadPosition>& Line, double S0, double S1) { }

	FRoadLaneMarkProfile() = default;
	FRoadLaneMarkProfile(ERoadLaneMark InType)
		: Type(InType)
	{
	}

	FRoadLaneMarkProfile& SetType(ERoadLaneMark InType) { Type = InType; return *this; }
};

USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneMarkProfileSolid : public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	FColor VertexColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	FRoadLaneMarkProfileSolid()
		:FRoadLaneMarkProfile(ERoadLaneMark::Solid)
	{
	}

	FRoadLaneMarkProfileSolid(double InWidth, const FColor& InColor = FColor::White)
		: Width(InWidth)
		, VertexColor(InColor)
	{
	}

	FRoadLaneMarkProfile& SetWidth(double InWidth) { Width = InWidth; return *this; }
	FRoadLaneMarkProfile& SetColor(const FColor& InVertexColor) { VertexColor = InVertexColor; return *this; }

};

USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneMarkProfileBroked : public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Width = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Long = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Gap = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	FColor VertexColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	FRoadLaneMarkProfileBroked()
		:FRoadLaneMarkProfile(ERoadLaneMark::Broked)
	{
	}

	FRoadLaneMarkProfileBroked(double InWidth, double InLong, double InGap, const FColor& InColor = FColor::White)
		: Width(InWidth)
		, Long(InLong)
		, Gap(InGap)
		, VertexColor(InColor)
	{
	}

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	//TSoftObjectPtr<UMaterialInterface> Material;
	FRoadLaneMarkProfile& SetWidth(double InWidth) { Width = InWidth; return *this; }
	FRoadLaneMarkProfile& SetColor(const FColor& InVertexColor) { VertexColor = InVertexColor; return *this; }
	FRoadLaneMarkProfile& SetLong(double InLong) { Long = InLong; return *this; }
	FRoadLaneMarkProfile& SetGap(double InGap) { Gap = InGap; return *this; }
};


USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneMarkProfileDouble : public FRoadLaneMarkProfile
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, NoClear, Meta = (ExcludeBaseStruct))
	TInstancedStruct<FRoadLaneMarkProfile> Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile, NoClear, Meta = (ExcludeBaseStruct))
	TInstancedStruct<FRoadLaneMarkProfile> Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Profile)
	double Gap = 400;

	FRoadLaneMarkProfileDouble() = default;
	FRoadLaneMarkProfileDouble(ERoadLaneMark InType, TInstancedStruct<FRoadLaneMarkProfile>&& InLeft, TInstancedStruct<FRoadLaneMarkProfile>&& InRight, double InGap)
		: FRoadLaneMarkProfile(InType)
		, Left(MoveTemp(InLeft))
		, Right(MoveTemp(InRight))
		, Gap(InGap)
	{
	}
	FRoadLaneMarkProfileDouble(ERoadLaneMark InType, const TInstancedStruct<FRoadLaneMarkProfile>& InLeft, const TInstancedStruct<FRoadLaneMarkProfile>& InRight, double InGap)
		: FRoadLaneMarkProfile(InType)
		, Left(InLeft)
		, Right(InRight)
		, Gap(InGap)
	{
	}

	FRoadLaneMarkProfile& SetLeft(TInstancedStruct<FRoadLaneMarkProfile>&& InLeft) { Left = MoveTemp(InLeft); return *this; }
	FRoadLaneMarkProfile& SetRight(TInstancedStruct<FRoadLaneMarkProfile>&& InRight) { Right = MoveTemp(InRight); return *this; }
	FRoadLaneMarkProfile& SetLeft(const TInstancedStruct<FRoadLaneMarkProfile>& InLeft) { Left = InLeft; return *this; }
	FRoadLaneMarkProfile& SetRight(const TInstancedStruct<FRoadLaneMarkProfile>& InRight) { Right = InRight; return *this; }
	FRoadLaneMarkProfile& SetGap(double InGap) { Gap = InGap; return *this; }
};

/**
 * ERoadLaneMarkProfile
 */
UENUM(BlueprintType)
enum class ERoadLaneMarkProfile : uint8
{
	UsePreset,
	UseCustom
};

/**
 * Used to stores information about road lane markings and used in procedural generation in the Build Mesh Modeling tool.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneMark : public FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()

	FRoadLaneMark();
	virtual ~FRoadLaneMark() {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey)
	ERoadLaneMarkProfile ProfileSource = ERoadLaneMarkProfile::UsePreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttributeKey, meta = (EditCondition = "ProfileSource == ERoadLaneMarkProfile::UsePreset", EditConditionHides))
	TSoftObjectPtr<class URoadMarkProfile> Profile;

	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (InlineEditConditionToggle))
	bool bOverrideMaterial = false;

	/** Override default material from profile */
	UPROPERTY(EditAnywhere, Category = AttributeKey, meta = (EditCondition = "ProfileSource == ERoadLaneMarkProfile::UsePreset && bOverrideMaterial", EditConditionHides))
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	/**
	 * Use a custom profile if ProfileSource = ERoadLaneMarkProfile::UseCustom.
	 * ATTENTION BUG: If this option is disabled in UI, just reselect the attribute key. This is a UE bug. Hope it will be fixed in future versions of UE
	 */
	UPROPERTY(EditAnywhere, Category = AttributeKey, Meta = (ExcludeBaseStruct, EditCondition = "ProfileSource == ERoadLaneMarkProfile::UseCustom", EditConditionHides))
	TInstancedStruct<FRoadLaneMarkProfile> CustomProfile;

	const TInstancedStruct<FRoadLaneMarkProfile>& GetProfile() const;
	TInstancedStruct<FRoadLaneMarkProfile>& GetProfile();

	virtual double GetKeyAlpha() const override { return 1.0; }
};

/**
 * Used to stores information about road lane markings and used in procedural generation in the Build Mesh Modeling tool.
 */
UCLASS(BlueprintType, DisplayName = "Mark")
class METAROAD_API URoadLaneAttributeMarkDescriptor : public URoadLaneAttributeDescriptor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FRoadLaneMark AttributeValueTemplate;

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const override { return AttributeValueTemplate; }

#if WITH_EDITOR
	virtual bool CanBeAdded() const override { return true; }
	virtual void Draw(const FAttributeDrawParams& Params) const override;
	virtual FColor GetDefaultColor() const override { return FColor::White; }
#endif
};

