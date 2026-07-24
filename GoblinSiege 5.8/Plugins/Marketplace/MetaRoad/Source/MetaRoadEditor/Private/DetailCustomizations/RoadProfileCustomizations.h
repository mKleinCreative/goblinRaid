// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"
#include "DetailLayoutBuilder.h"

class FReply;
class SButton;
class SWidget;
enum class ECheckBoxState : uint8;

class IDetailLayoutBuilder;
class IPropertyHandle;
class IDetailChildrenBuilder;
class SComboButton;
class SComboPanel;
class SBox;
class UMeshVertexSculptTool;
class FRecentAlphasProvider;
class SToolInputAssetComboPanel;

/*


class FRoadProfileDetails : public IDetailCustomization
{
public:
	virtual ~FRoadProfileDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TSharedPtr<FRecentAlphasProvider> RecentAlphasProvider;

protected:
	TWeakObjectPtr<UMeshVertexSculptTool> TargetTool;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	TSharedPtr<SToolInputAssetComboPanel> AlphaAssetPicker;
	FDelegateHandle AlphaTextureUpdateHandle;
};

*/