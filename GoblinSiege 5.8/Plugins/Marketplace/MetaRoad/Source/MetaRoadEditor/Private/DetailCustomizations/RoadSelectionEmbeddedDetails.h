/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * FRoadSelectionEmbeddedDetails
 *
 * Per-view details customization used ONLY by the mode toolkit's embedded "Selection" details view
 * (registered on that view via IDetailsView::RegisterInstancedCustomPropertyLayout). It shows just the
 * active edit sub-mode editor (Spline/Section/Offset/Width/Attribute) for the selected road spline and
 * hides every other category, so the left panel isn't cluttered with the component's Transform/Landscape/
 * etc. The main (right) Details panel keeps using FRoadSplineComponentDetails (SubGroup row).
 */
class FRoadSelectionEmbeddedDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
