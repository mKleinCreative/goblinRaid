/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once
#include "Engine/DataAsset.h"
#include "RoadLaneAttribute.h"
#include "StructUtils/StructView.h"
#include "Engine/Blueprint.h"
#include "RoadLaneAttributeDescriptor.generated.h"

class URoadSplineComponent;
class FPrimitiveDrawInterface;
struct FAssetOpenArgs;

/**
 * This class is used to register new road lane attribute types in UE. See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class METAROAD_API URoadLaneAttributeDescriptor : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FText LabelOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RoadAttribute)
	FText ToolTipOverride;
#endif

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttributeBeginDestroyDelegate, URoadLaneAttributeDescriptor*);
	static FOnAttributeBeginDestroyDelegate OnAttributeBeginDestroy;

public:
	virtual void BeginDestroy() override;

	virtual TConstStructView<FRoadLaneAttributeValue> GetAttributeValueTemplate() const;

#if WITH_EDITOR

	virtual FText GetDisplayName() const;
	virtual FText GetToolTip() const;
	virtual FText GetAssetDisplayName() const;
	virtual FSlateIcon GetIcon() const;
	virtual bool CanBeAdded() const;
	virtual bool OpenCustomAssetEditor(const FAssetOpenArgs& OpenArgs)  { return false; }

	struct METAROAD_API FAttributeDrawParams
	{
		FPrimitiveDrawInterface* PDI;
		const URoadSplineComponent* Spline;
		const FRoadLaneAttribute* Attribute;
		int SectionIndex;
		int LaneIndex;
		int AttributeIndex;
		bool bLowAccent;
		bool bIsAttributeSelected;

		TPair<double, double> GetRang() const;
	};

	virtual void Draw(const FAttributeDrawParams& Params) const;
	virtual void DrawKey(const FAttributeDrawParams& Params) const;
	virtual FColor GetDefaultColor() const;
	virtual bool CanBeAddedTo(const URoadSplineComponent* Spline, int SectionIndex, int LaneIndex) const;

#endif
};

UCLASS()
class METAROAD_API URoadLaneAttributeDescriptorBlueprint : public UBlueprint
{
	GENERATED_BODY()

	// UBlueprint interface
	//virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	//virtual bool AlwaysCompileOnLoad() const override { return true; }
	// End of UBlueprint interface
};

