/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadToolPresetManager.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"
#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"
#include "Assets/RoadBuildPreset.h"
#include "Assets/RoadBuildPreset/RoadBuildPresetFactory.h"

#include "Editor.h"
#include "Engine/Engine.h" // UEngine::CopyPropertiesForUnrelatedObjects
#include "Tools/UEdMode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Textures/SlateIcon.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FMetaRoadToolPresetManager"

// --- FEdModePresetContext --------------------------------------------------------------------------------

TArray<UMetaRoadBuildSettings*> FEdModePresetContext::GetPresetTargets() const
{
	TArray<UMetaRoadBuildSettings*> Targets;
	if (UMetaRoadEditorMode* Mode = Cast<UMetaRoadEditorMode>(EdMode.Get()))
	{
		for (UObject* HolderObject : Mode->GetPresetWorkingObjects())
		{
			if (UMetaRoadBuildSettings* Holder = Cast<UMetaRoadBuildSettings>(HolderObject))
			{
				Targets.Add(Holder);
			}
		}
	}
	return Targets;
}

// --- FMetaRoadToolPresetManager --------------------------------------------------------------------------

FMetaRoadToolPresetManager::FMetaRoadToolPresetManager(TSharedRef<IMetaRoadPresetContext> InContext)
	: Context(InContext)
{
}

void FMetaRoadToolPresetManager::CopySettings(const UMetaRoadBuildSettings& Source, UMetaRoadBuildSettings& Target)
{
	for (const FMetaRoadBuildPropertySetInfo& Info : UMetaRoadBuildSettings::GetBuildPropertySetInfos())
	{
		if (!Info.PropsClass)
		{
			continue;
		}
		UMetaRoadBuildSettingsBase* Src = Source.FindPropertySet(Info.PropsClass);
		if (!Src)
		{
			continue;
		}
		UMetaRoadBuildSettingsBase* Dst = Target.FindOrCreatePropertySet(Info.PropsClass);
		if (Dst)
		{
			// bDoDelta=false: copy every property verbatim. With the default delta serialization, a source
			// value equal to the class default (e.g. an empty array) is skipped, so the destination would
			// keep its own non-empty value — meaning a preset could never clear an array.
			UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
			Params.bDoDelta = false;
			UEngine::CopyPropertiesForUnrelatedObjects(Src, Dst, Params);
		}
	}
}

TArray<FAssetData> FMetaRoadToolPresetManager::GetAvailablePresets() const
{
	TArray<FAssetData> Assets;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassPaths.Add(URoadBuildPreset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	return Assets;
}

void FMetaRoadToolPresetManager::ApplyPreset(URoadBuildPreset* Preset)
{
	if (!Preset || !Preset->Settings)
	{
		return;
	}
	for (UMetaRoadBuildSettings* Target : Context->GetPresetTargets())
	{
		if (Target)
		{
			CopySettings(*Preset->Settings, *Target);
		}
	}
	SelectedPreset = Preset;
	OnPresetApplied.Broadcast();
}

void FMetaRoadToolPresetManager::ApplyPreset_ByPath(FSoftObjectPath PresetPath)
{
	ApplyPreset(Cast<URoadBuildPreset>(PresetPath.TryLoad()));
}

void FMetaRoadToolPresetManager::SaveAsNewPreset()
{
	const TArray<UMetaRoadBuildSettings*> Targets = Context->GetPresetTargets();
	if (Targets.Num() == 0 || !Targets[0])
	{
		return;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	URoadBuildPresetFactory* Factory = NewObject<URoadBuildPresetFactory>();
	UObject* NewAsset = AssetTools.CreateAssetWithDialog(URoadBuildPreset::StaticClass(), Factory);
	if (URoadBuildPreset* Preset = Cast<URoadBuildPreset>(NewAsset))
	{
		// Capture the (primary) target's current settings into the new preset.
		Preset->Settings = DuplicateObject<UMetaRoadBuildSettings>(Targets[0], Preset);
		Preset->MarkPackageDirty();
		SelectedPreset = Preset;
	}
}

void FMetaRoadToolPresetManager::UpdateSelectedPreset()
{
	const TArray<UMetaRoadBuildSettings*> Targets = Context->GetPresetTargets();
	URoadBuildPreset* Preset = SelectedPreset.Get();
	if (!Preset || Targets.Num() == 0 || !Targets[0])
	{
		return;
	}
	Preset->Modify();
	Preset->Settings = DuplicateObject<UMetaRoadBuildSettings>(Targets[0], Preset);
	Preset->MarkPackageDirty();
}

bool FMetaRoadToolPresetManager::CanUpdateSelectedPreset() const
{
	return SelectedPreset.IsValid() && Context->GetPresetTargets().Num() > 0;
}

bool FMetaRoadToolPresetManager::IsPresetSelected(FSoftObjectPath PresetPath) const
{
	const URoadBuildPreset* Preset = SelectedPreset.Get();
	return Preset && FSoftObjectPath(Preset) == PresetPath;
}

FText FMetaRoadToolPresetManager::GetSelectedPresetLabel() const
{
	if (const URoadBuildPreset* Preset = SelectedPreset.Get())
	{
		return FText::FromString(Preset->GetName());
	}
	return LOCTEXT("NoPresetSelected", "None Selected");
}

FText FMetaRoadToolPresetManager::GetSelectedPresetTooltip() const
{
	if (const URoadBuildPreset* Preset = SelectedPreset.Get())
	{
		return FText::Format(
			LOCTEXT("PresetSelectedTip", "Active build preset: {0}. Click to apply another or manage presets."),
			FText::FromString(Preset->GetName()));
	}
	return LOCTEXT("NoPresetSelectedTip", "No build preset applied. Click to pick a preset to apply its build settings.");
}

TSharedRef<SWidget> FMetaRoadToolPresetManager::MakePresetPickerMenu()
{
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	// Apply section: one entry per available preset asset (or a placeholder when there are none).
	const TArray<FAssetData> Presets = GetAvailablePresets();
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ApplyPresetSection", "Apply Preset"));
	if (Presets.Num() == 0)
	{
		MenuBuilder.AddWidget(SNew(STextBlock).Text(LOCTEXT("NoPresets", "No build presets")), FText::GetEmpty());
	}
	else
	{
		for (const FAssetData& AssetData : Presets)
		{
			const FSoftObjectPath AssetPath = AssetData.ToSoftObjectPath();
			// RadioButton entry: the currently-applied preset shows a filled indicator (only one active at a time).
			MenuBuilder.AddMenuEntry(
				FText::FromName(AssetData.AssetName),
				FText::FromString(AssetData.GetObjectPathString()),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FMetaRoadToolPresetManager::ApplyPreset_ByPath, AssetPath),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &FMetaRoadToolPresetManager::IsPresetSelected, AssetPath)),
				NAME_None,
				EUserInterfaceActionType::RadioButton);
		}
	}
	MenuBuilder.EndSection();

	// Manage section (Save As / Update) — only where the context allows it (e.g. not the apply-only cases).
	if (Context->AllowsManagement())
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ManagePresetSection", "Manage"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SaveAsPreset", "Save As New"),
			LOCTEXT("SaveAsPresetTip", "Save the current build settings as a new Road Build Preset asset."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FMetaRoadToolPresetManager::SaveAsNewPreset)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("UpdatePreset", "Save to Selected"),
			LOCTEXT("UpdatePresetTip", "Overwrite the selected preset with the current build settings."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaRoadToolPresetManager::UpdateSelectedPreset),
				FCanExecuteAction::CreateSP(this, &FMetaRoadToolPresetManager::CanUpdateSelectedPreset)));
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> FMetaRoadToolPresetManager::MakePresetPanel(FName ComboButtonStyleName)
{
	// Single combo: picking a preset applies it; Save As / Update live in the combo's Manage section.
	return SNew(SBox)
		.Visibility_Lambda([this]() { return Context->GetPresetTargets().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SComboButton)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>(ComboButtonStyleName))
			.HasDownArrow(true)
			.ToolTipText(this, &FMetaRoadToolPresetManager::GetSelectedPresetTooltip)
			.OnGetMenuContent(this, &FMetaRoadToolPresetManager::MakePresetPickerMenu)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				// Favorite/star icon on the left (Starship/Common/Favorite, registered as "Icons.Star").
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Star"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(this, &FMetaRoadToolPresetManager::GetSelectedPresetLabel)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
