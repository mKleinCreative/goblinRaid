/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/Widgets/SMetaRoadBakePanel.h"
#include "EditorMode/Widgets/MetaRoadWidgetUtils.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "EditorMode/MetaRoadBakeSettings.h"

#include "IDetailsView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SMetaRoadBakePanel"

void SMetaRoadBakePanel::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	// "Selected" buttons are enabled only when a road actor is selected; all are disabled while baking.
	auto MakeButton = [this](const FText& Label, const FText& Tip, bool bSelectedOnly, auto Action) -> TSharedRef<SWidget>
	{
		TSharedRef<SButton> Button = SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(Label)
			.ToolTipText(Tip)
			.OnClicked_Lambda([this, Action]() -> FReply
			{
				if (UMetaRoadEditorMode* Mode = EditorMode.Get())
				{
					Action(Mode);
				}
				return FReply::Handled();
			});
		Button->SetEnabled(TAttribute<bool>::CreateLambda([this, bSelectedOnly]()
		{
			UMetaRoadEditorMode* Mode = EditorMode.Get();
			return Mode && !Mode->IsBaking() && (!bSelectedOnly || Mode->HasSelectedRoad());
		}));
		return Button;
	};

	// Bake "Generated Assets" settings (location / mode / auto-gen path…), edited on the config-backed CDO.
	BakeSettingsView = MetaRoadWidgetUtils::MakeCompactDetailsView();
	BakeSettingsView->SetObject(UMetaRoadBakeSettings::Get());

	ChildSlot
	[
		SNew(SVerticalBox)
		// Settings. FillHeight (not AutoHeight) so the details view gets a bounded height and shows its own
		// scrollbar when the properties overflow — the toolkit strips the outer mode-panel scroll (see
		// FMetaRoadEditorModeToolkit::InvokeUI), so an AutoHeight details view would clip instead of scroll.
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 2.f, 0.f, 4.f))
		[
			BakeSettingsView->AsShared()
		]
		// Bake row.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.f, 4.f, 5.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("BakeSelected", "Bake Selected"),
					LOCTEXT("BakeSelectedTip", "Generate road mesh assets/actors for the selected road actors."),
					/*bSelectedOnly=*/true, [](UMetaRoadEditorMode* M) { M->BakeSelected(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("BakeAll", "Bake All"),
					LOCTEXT("BakeAllTip", "Generate road mesh assets/actors for every road actor in the level."),
					/*bSelectedOnly=*/false, [](UMetaRoadEditorMode* M) { M->BakeAll(); })
			]
		]
		// Clear row.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.f, 2.f, 5.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("ClearSelected", "Clear Selected"),
					LOCTEXT("ClearSelectedTip", "Delete the generated mesh actors/assets for the selected road actors."),
					/*bSelectedOnly=*/true, [](UMetaRoadEditorMode* M) { M->ClearSelected(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("ClearAll", "Clear All"),
					LOCTEXT("ClearAllTip", "Delete the generated mesh actors/assets for every road actor in the level."),
					/*bSelectedOnly=*/false, [](UMetaRoadEditorMode* M) { M->ClearAll(); })
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
