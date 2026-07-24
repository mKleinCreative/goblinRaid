/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UEdMode;
class SWidget;
class UMetaRoadBuildSettings;
class URoadBuildPreset;
struct FAssetData;

/**
 * Source of the build-settings holder(s) that road build presets (URoadBuildPreset) apply to / capture
 * from. Decouples FMetaRoadToolPresetManager from the editor mode and the Road Profile editor.
 */
class IMetaRoadPresetContext
{
public:
	virtual ~IMetaRoadPresetContext() = default;

	/** Holders a preset applies to (one per selected road). Capture (Save As/Update) uses the first.
	 *  Empty = nothing to apply to (the preset panel hides). */
	virtual TArray<UMetaRoadBuildSettings*> GetPresetTargets() const = 0;

	/** false = apply-only (no Save As / Update) — e.g. the Road Profile editor preview. */
	virtual bool AllowsManagement() const { return true; }
};

/** Preset context backed by the Preset sub-mode's working copies of the selected actors' build settings. */
class FEdModePresetContext : public IMetaRoadPresetContext
{
public:
	explicit FEdModePresetContext(TWeakObjectPtr<UEdMode> InEdMode) : EdMode(InEdMode) {}
	virtual TArray<UMetaRoadBuildSettings*> GetPresetTargets() const override;

private:
	TWeakObjectPtr<UEdMode> EdMode;
};

/**
 * Owns the "Presets" panel for road build settings: pick a URoadBuildPreset asset to Apply, and (when the
 * context allows management) Save As New / Update. Presets are first-class assets shared across the editor
 * mode and the Road Profile editor.
 */
class FMetaRoadToolPresetManager : public TSharedFromThis<FMetaRoadToolPresetManager>
{
public:
	explicit FMetaRoadToolPresetManager(TSharedRef<IMetaRoadPresetContext> InContext);

	/** Builds the "Presets" panel widget.
	 *  @param ComboButtonStyleName  FAppStyle FComboButtonStyle for the picker button. Defaults to the
	 *         borderless "SimpleComboButton" (flat, reads as raw text); pass "ComboButton" for a framed button. */
	TSharedPtr<SWidget> MakePresetPanel(FName ComboButtonStyleName = TEXT("SimpleComboButton"));

	/** Broadcast after a preset's values are applied to the context's targets (host rebuilds/dirties). */
	FSimpleMulticastDelegate OnPresetApplied;

private:
	TSharedRef<IMetaRoadPresetContext> Context;
	TWeakObjectPtr<URoadBuildPreset> SelectedPreset;

	TSharedRef<SWidget> MakePresetPickerMenu();
	FText GetSelectedPresetLabel() const;
	FText GetSelectedPresetTooltip() const;

	void ApplyPreset(URoadBuildPreset* Preset);
	void ApplyPreset_ByPath(FSoftObjectPath PresetPath);
	void SaveAsNewPreset();
	void UpdateSelectedPreset();
	bool CanUpdateSelectedPreset() const;
	/** True when the given preset asset is the one currently applied — drives the radio check in the picker menu. */
	bool IsPresetSelected(FSoftObjectPath PresetPath) const;

	/** All URoadBuildPreset assets in the project (via the asset registry). */
	TArray<FAssetData> GetAvailablePresets() const;

	/** Copy Preset->Settings into the given target holder (value copy per property-set class). */
	static void CopySettings(const UMetaRoadBuildSettings& Source, UMetaRoadBuildSettings& Target);
};
