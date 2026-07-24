/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaRoadBuildSettingsBase.generated.h"

/**
 * UMetaRoadBuildSettingsBase
 *
 * Plain base for a road mesh-build property set (triangulation params + each layer's settings). Replaces
 * the former UInteractiveToolPropertySet base: these are serialized per-actor build data, not interactive
 * tool UI, so they only need the "a property changed" notification that drives the live-preview rebuild.
 *
 * GetOnModified() mirrors UInteractiveToolPropertySet::GetOnModified() (same signature) so the preview
 * manager subscription is unchanged; it is broadcast from PostEditChangeProperty.
 */
UCLASS(Abstract, EditInlineNew)
class METAROADEDITOR_API UMetaRoadBuildSettingsBase : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBuildSettingsModified, UObject* /*PropertySet*/, FProperty* /*Property*/);

	/** Broadcast from PostEditChangeProperty (editor) — consumed by the live-preview rebuild. */
	FOnBuildSettingsModified& GetOnModified() { return OnModified; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FOnBuildSettingsModified OnModified;
};
