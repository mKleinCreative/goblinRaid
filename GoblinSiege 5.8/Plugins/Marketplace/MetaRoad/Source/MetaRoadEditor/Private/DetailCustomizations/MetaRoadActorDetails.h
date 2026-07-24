/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;
class UMetaRoadBuildSettings;
class SWindow;
class AMetaRoad;

namespace MetaRoadBuildSettingsDetails
{
	/** Inject the given build-settings holders' property sets into the layout as flat per-layer categories
	 *  (Triangulation, Drive Surface, Decals, ...), driven by UMetaRoadBuildSettings::GetBuildPropertySetInfos().
	 *  Same-class property sets across holders are grouped for multi-edit. Shared by the customizations below. */
	void FlattenHoldersIntoCategories(IDetailLayoutBuilder& DetailBuilder, const TArray<UMetaRoadBuildSettings*>& Holders);
}

/**
 * FMetaRoadActorDetails
 *
 * Details customization for AMetaRoad. Ensures the per-actor UMetaRoadBuildSettings holder exists (default
 * object), hides the raw instanced holder property (so the class can't be changed/cleared), and replaces it
 * with an "Edit..." button that opens the holder's Details in a separate floating window (which renders the
 * settings flat via the registered FMetaRoadBuildSettingsDetails).
 */
class FMetaRoadActorDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply OpenBuildSettingsWindow(TArray<TWeakObjectPtr<UMetaRoadBuildSettings>> Holders);

	/** Saves the given road actor as a Blueprint template via UMetaRoadBlueprintLibrary::SaveAsTemplate. */
	FReply OnSaveAsTemplateClicked(TWeakObjectPtr<AMetaRoad> Road);

	/** The currently open settings window, if any — reused/focused instead of opening duplicates. */
	TWeakPtr<SWindow> SettingsWindow;
};

/**
 * FMetaRoadBuildSettingsDetails
 *
 * Details customization for UMetaRoadBuildSettings shown directly (not via an actor) — used by the Preset
 * panel's Details view over the transient working copies. Flattens the holder's property sets the same way.
 */
class FMetaRoadBuildSettingsDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
