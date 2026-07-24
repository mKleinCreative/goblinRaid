/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */
#pragma once

#include "Script/AssetDefinition_Blueprint.h"
#include "RoadLaneAttributeDescriptorAction.generated.h"

UCLASS(MinimalAPI)
class UAssetDefinition_RoadLaneAttributeDescriptor : public UAssetDefinition_Blueprint
{
	GENERATED_BODY()
public:
	virtual FText GetAssetDisplayName() const override;
	virtual FText GetAssetDisplayName(const FAssetData& AssetData) const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
