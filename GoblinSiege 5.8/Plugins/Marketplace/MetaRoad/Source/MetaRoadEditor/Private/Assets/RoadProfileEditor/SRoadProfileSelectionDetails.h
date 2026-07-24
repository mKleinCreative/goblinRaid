/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class URoadProfile;
class URoadProfilePreviewBuilder;
class IStructureDetailsView;
class IDetailsView;
class FStructOnScope;
class SWidgetSwitcher;
struct FPropertyAndParent;
struct FPropertyChangedEvent;

/**
 * SRoadProfileSelectionDetails
 *
 * Selection-driven details for the Road Profile editor (analog of FRoadSelectionDetails, but without
 * IDetailCustomNodeBuilder). Shows:
 *   - a selected lane     -> stock IStructureDetailsView on its FRoadLaneProfile (edited in place),
 *   - the center lane     -> the profile's road-level props (Direction + CenterAttributes),
 *   - nothing selected    -> the whole URoadProfile.
 *
 * The lane attribute set (TSet<FRoadLaneAttributeProfile>) renders via the already-registered
 * FRoadLaneAttributeProfileDetails customization.
 */
class SRoadProfileSelectionDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadProfileSelectionDetails) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, URoadProfile* InProfile, URoadProfilePreviewBuilder* InBuilder);
	virtual ~SRoadProfileSelectionDetails() override;

	/** Rebuild the shown details from the builder's current lane selection. */
	void Refresh();

private:
	bool IsProfilePropertyVisible(const FPropertyAndParent& PropertyAndParent) const;
	void OnDetailsChanged(const FPropertyChangedEvent& Event);

	TWeakObjectPtr<URoadProfile> Profile;
	TWeakObjectPtr<URoadProfilePreviewBuilder> Builder;

	TSharedPtr<IStructureDetailsView> LaneView;   // a selected lane (FRoadLaneProfile, in place)
	TSharedPtr<IDetailsView> ProfileView;          // center (filtered) / whole profile
	TSharedPtr<FStructOnScope> LaneScope;
	TSharedPtr<SWidgetSwitcher> Switcher;

	FDelegateHandle SelectionHandle;
};
