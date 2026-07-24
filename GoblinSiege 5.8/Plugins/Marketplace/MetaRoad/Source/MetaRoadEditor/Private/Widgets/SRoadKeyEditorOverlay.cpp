/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "Widgets/SRoadKeyEditorOverlay.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ModelingWidgets/SDraggableBox.h"

#define LOCTEXT_NAMESPACE "SRoadKeyEditorOverlay"

TSharedRef<SWidget> RoadKeyOverlay::MakeNumericRow(
	FText Label,
	TAttribute<TOptional<double>> Get,
	SNumericEntryBox<double>::FOnValueCommitted OnCommitted,
	TAttribute<bool> IsEnabled,
	TOptional<double> MinValue,
	TOptional<double> MaxValue)
{
	return SNew(SHorizontalBox)
		.IsEnabled(IsEnabled)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 2.f, 8.f, 2.f)
		[
			SNew(SBox)
			.MinDesiredWidth(56.f)
			[
				SNew(STextBlock).Text(Label)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(0.f, 2.f)
		[
			SNew(SNumericEntryBox<double>)
			.AllowSpin(false)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Value(Get)
			.OnValueCommitted(OnCommitted)
		];
}

// Default starting position: 16px inset from the bottom-right corner of the viewport.
float SRoadKeyEditorOverlay::SavedPaddingRight = 16.f;
float SRoadKeyEditorOverlay::SavedPaddingBottom = 16.f;

void SRoadKeyEditorOverlay::Construct(const FArguments& InArgs)
{
	// The container itself never blocks viewport input; only the actual fields do.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// Restore the position last set by the user (persists across visualizer/widget recreation).
	PaddingRight = SavedPaddingRight;
	PaddingBottom = SavedPaddingBottom;

	const TAttribute<FText> HeaderTextAttr = InArgs._HeaderText;

	TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox);

	// Header row: bound text, collapses itself when the text is empty.
	Inner->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 4.f)
	[
		SNew(STextBlock)
		.Text(HeaderTextAttr)
		.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
		.Visibility(TAttribute<EVisibility>::Create([HeaderTextAttr]()
		{
			return HeaderTextAttr.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		}))
	];

	Inner->AddSlot()
	.AutoHeight()
	[
		InArgs._Content.Widget
	];

	// Slot padding follows the current (live) position, anchored to the bottom-right corner.
	// Persists the position via the static fields below.
	TAttribute<FMargin> PaddingAttr = TAttribute<FMargin>::Create([this]()
	{
		return FMargin(0.f, 0.f, PaddingRight, PaddingBottom);
	});

	// SDraggableBox (ModelingWidgets) is deprecated as of UE 5.7 in favour of
	// UE::ToolWidgets::SDraggableBoxOverlay, which does not exist in 5.6. To keep a single code path
	// that compiles cleanly on both engine versions we keep using SDraggableBox and silence the 5.7
	// deprecation warning here. Migrate to SDraggableBoxOverlay once 5.6 support is dropped.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SDraggableBox::FOnDragComplete OnDragComplete = SDraggableBox::FOnDragComplete::CreateLambda(
		[this](const FVector2D& ScreenSpacePosition)
	{
		if (!ContainingBox.IsValid() || !DraggableBox.IsValid())
		{
			return;
		}
		// ScreenSpacePosition is the dragged box's top-left corner in screen space.
		const FVector2D BoxAbsSize = DraggableBox->GetTickSpaceGeometry().GetAbsoluteSize();
		const FVector2D BottomRightScreen(
			ScreenSpacePosition.X + BoxAbsSize.X,
			ScreenSpacePosition.Y + BoxAbsSize.Y);
		const FVector2D BottomRightLocal = ContainingBox->GetTickSpaceGeometry().AbsoluteToLocal(BottomRightScreen);
		const FVector2D LocalSize = ContainingBox->GetTickSpaceGeometry().GetLocalSize();

		PaddingRight = LocalSize.X - BottomRightLocal.X;
		PaddingBottom = LocalSize.Y - BottomRightLocal.Y;

		// Persist for the next overlay instance (other visualizers).
		SavedPaddingRight = PaddingRight;
		SavedPaddingBottom = PaddingBottom;
	});

	const TAttribute<EVisibility> ContentVisibility =
		(InArgs._OverlayVisibility.IsBound() || InArgs._OverlayVisibility.IsSet())
		? InArgs._OverlayVisibility
		: TAttribute<EVisibility>(EVisibility::Visible);

	ChildSlot
	[
		SAssignNew(ContainingBox, SVerticalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(PaddingAttr)
		[
			SAssignNew(DraggableBox, SDraggableBox)
			.OnDragComplete(OnDragComplete)
			[
				SNew(SBorder)
				// Toggling the frame (not SDraggableBox itself) keeps the box's own drag
				// visibility handling intact.
				.Visibility(ContentVisibility)
				.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
				.Padding(8.f)
				[
					SNew(SBox)
					.MinDesiredWidth(180.f)
					[
						Inner
					]
				]
			]
		]
	];
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

// -------------------------------------------------------------------------------------------------------------------------------------------------

FRoadKeyOverlayController::~FRoadKeyOverlayController()
{
	Shutdown();
}

void FRoadKeyOverlayController::Setup(TSharedRef<SWidget> Content, TAttribute<FText> Header, TAttribute<EVisibility> OverlayVisibility)
{
	Overlay = SNew(SRoadKeyEditorOverlay)
		.HeaderText(Header)
		.OverlayVisibility(OverlayVisibility)
		[
			Content
		];
}

void FRoadKeyOverlayController::EnsureAddedToActiveViewport()
{
	if (!Overlay.IsValid())
	{
		return;
	}

	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
	if (!LevelEditor)
	{
		return;
	}

	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetFirstActiveLevelViewport();
	if (!ActiveViewport.IsValid())
	{
		return;
	}

	TSharedPtr<SLevelViewport> AddedPin = AddedViewport.Pin();
	if (AddedPin == ActiveViewport)
	{
		return;
	}

	if (AddedPin.IsValid())
	{
		AddedPin->RemoveOverlayWidget(Overlay.ToSharedRef());
	}

	ActiveViewport->AddOverlayWidget(Overlay.ToSharedRef());
	AddedViewport = ActiveViewport;
}

void FRoadKeyOverlayController::Shutdown()
{
	if (Overlay.IsValid())
	{
		if (TSharedPtr<SLevelViewport> AddedPin = AddedViewport.Pin())
		{
			AddedPin->RemoveOverlayWidget(Overlay.ToSharedRef());
		}
	}
	AddedViewport.Reset();
}

#undef LOCTEXT_NAMESPACE
