/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/Widgets/SMetaRoadFbxExportPanel.h"
#include "EditorMode/Widgets/MetaRoadWidgetUtils.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "EditorMode/MetaRoadFbxExportSettings.h"

#include "IDetailsView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SMetaRoadFbxExportPanel"

void SMetaRoadFbxExportPanel::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	// "Export Selected" is enabled only when a road actor is selected; both are disabled while baking.
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
			return Mode && !Mode->IsBaking() && !Mode->IsExporting() && (!bSelectedOnly || Mode->HasSelectedRoad());
		}));
		return Button;
	};

	// FBX export settings, edited on the config-backed CDO.
	ExportSettingsView = MetaRoadWidgetUtils::MakeCompactDetailsView();
	ExportSettingsView->SetObject(UMetaRoadFbxExportSettings::Get());

	ChildSlot
	[
		SNew(SVerticalBox)
		// Settings. FillHeight (not AutoHeight) so the details view gets a bounded height and shows its own
		// scrollbar when the properties overflow — the toolkit strips the outer mode-panel scroll (see
		// FMetaRoadEditorModeToolkit::InvokeUI), so an AutoHeight details view would clip instead of scroll.
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 2.f, 0.f, 4.f))
		[
			ExportSettingsView->AsShared()
		]
		// Export row.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.f, 4.f, 5.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("ExportSelected", "Export Selected"),
					LOCTEXT("ExportSelectedTip", "Export the baked meshes of the selected road actors to FBX."),
					/*bSelectedOnly=*/true, [](UMetaRoadEditorMode* M) { M->ExportSelectedToFBX(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f, 0.f)
			[
				MakeButton(LOCTEXT("ExportAll", "Export All"),
					LOCTEXT("ExportAllTip", "Export the baked meshes of every road actor in the level to FBX."),
					/*bSelectedOnly=*/false, [](UMetaRoadEditorMode* M) { M->ExportAllToFBX(); })
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
