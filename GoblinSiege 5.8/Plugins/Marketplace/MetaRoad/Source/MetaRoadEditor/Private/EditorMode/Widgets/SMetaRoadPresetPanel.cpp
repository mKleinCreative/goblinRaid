/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/Widgets/SMetaRoadPresetPanel.h"
#include "EditorMode/Widgets/MetaRoadWidgetUtils.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "EditorMode/MetaRoadToolPresetManager.h"

#include "IDetailsView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SMetaRoadPresetPanel"

void SMetaRoadPresetPanel::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	// Preset manager sourced from the editor mode's working copies (FEdModePresetContext); applying a preset
	// rebuilds the live preview.
	PresetManager = MakeShared<FMetaRoadToolPresetManager>(MakeShared<FEdModePresetContext>(EditorMode.Get()));
	PresetManager->OnPresetApplied.AddSP(this, &SMetaRoadPresetPanel::OnPresetApplied);
	// "ComboButton" = framed style (bordered button) rather than the borderless "SimpleComboButton" default.
	TSharedPtr<SWidget> ToolPresetArea = PresetManager->MakePresetPanel(TEXT("ComboButton"));

	// Details view over the preview manager's transient working copies (flattened by
	// FMetaRoadBuildSettingsDetails). RefreshWorkingObjects re-points it when the working copies change.
	PresetDetailsView = MetaRoadWidgetUtils::MakeCompactDetailsView();

	ChildSlot
	[
		SNew(SVerticalBox)
		// Preset combo.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(5.f, 2.f, 5.f, 5.f))
		[
			ToolPresetArea.ToSharedRef()
		]
		// Working-copy build settings (multi-edit over the selected actors' working copies).
		+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Fill)
		[
			PresetDetailsView->AsShared()
		]
		
		// Commit the edits to the selected actors' real settings.
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(5.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("ApplyToSelected", "Apply To Selected"))
			.ToolTipText(LOCTEXT("ApplyToSelectedTip", "Write the edited build settings into the selected road actors."))
			.OnClicked_Lambda([this]() -> FReply
			{
				if (UMetaRoadEditorMode* Mode = EditorMode.Get())
				{
					Mode->ApplyPresetToSelected();
				}
				return FReply::Handled();
			})
		]
		
	];
}

void SMetaRoadPresetPanel::RefreshWorkingObjects()
{
	if (PresetDetailsView.IsValid())
	{
		if (UMetaRoadEditorMode* Mode = EditorMode.Get())
		{
			PresetDetailsView->SetObjects(Mode->GetPresetWorkingObjects());
		}
	}
}

void SMetaRoadPresetPanel::OnPresetApplied()
{
	if (UMetaRoadEditorMode* Mode = EditorMode.Get())
	{
		Mode->RequestPreviewRebuild();
	}
}

#undef LOCTEXT_NAMESPACE
