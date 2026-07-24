/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "SInstancedStructPicker.h"
#include "Textures/SlateIcon.h"

class IPropertyHandle;
class IPropertyUtilities;
class IAssetReferenceFilter;
class SComboButton;
class URoadLaneAttributeDescriptor;


class SRoadLaneAttributeProfilePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadLaneAttributeProfilePicker) { }
		//SLATE_EVENT(FOnStructPicked, OnStructPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> RoadLaneAttributeProfileProperty, TSharedPtr<IPropertyUtilities> InPropertyUtils);

	//FOnStructPicked OnStructPicked;

private:
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<IPropertyHandle> AttributeValueProperty;
	TSharedPtr<IPropertyHandle> AttributeDesctiptorProperty;
	TSharedPtr<IPropertyUtilities> PropUtils;

	struct FComboBoxContent
	{
		FText Lable;
		FText Tooltip;
		FSlateIcon Icon;
	};
	FComboBoxContent ComboBoxContent;

	TSharedRef<SWidget> GenerateStructPicker();
	void StructPicked(const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute);

	void SetComboBoxContent(const TSubclassOf<URoadLaneAttributeDescriptor>& Attribute);
};
