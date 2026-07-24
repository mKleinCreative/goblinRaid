/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "Misc/EngineVersionComparison.h"
#include "SceneView.h"

// ---------------------------------------------------------------------------------------------------------
// FViewMatrices accessor compatibility shims.
//
// UE 5.8 renamed the FViewMatrices matrix accessors and deprecated the old names:
//   GetViewMatrix()           -> GetWorldToView()
//   GetProjectionMatrix()     -> GetViewToClip()
//   GetViewProjectionMatrix() -> GetWorldToClip()
// The deprecated names still compile on 5.8 (with a warning) but are slated for removal.
// These shims call the new API on 5.8+ and the original names on 5.6 / 5.7, so the plugin
// stays warning-free on 5.8 while remaining backward compatible with 5.6 / 5.7.
// ---------------------------------------------------------------------------------------------------------

namespace MetaRoad::ViewCompat
{
	inline const FMatrix& GetWorldToView(const FViewMatrices& ViewMatrices)
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		return ViewMatrices.GetWorldToView();
#else
		return ViewMatrices.GetViewMatrix();
#endif
	}

	inline const FMatrix& GetViewToClip(const FViewMatrices& ViewMatrices)
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		return ViewMatrices.GetViewToClip();
#else
		return ViewMatrices.GetProjectionMatrix();
#endif
	}

	inline const FMatrix& GetWorldToClip(const FViewMatrices& ViewMatrices)
	{
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
		return ViewMatrices.GetWorldToClip();
#else
		return ViewMatrices.GetViewProjectionMatrix();
#endif
	}
}
