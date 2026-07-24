
/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/ToolShutdownOverlayWidgetDefault.h"
#include "EditorMode/MetaRoadEditorMode.h"
#include "RoadEditorCommands.h"
#include "MetaRoadEditorStyle.h"
#include "Toolkits/BaseToolkit.h"
#include "ModelingToolsEditorModeStyle.h"
#include "Tools/UEdMode.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "SPrimaryButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "IToolShutdownOverlayWidgetDefault"

const FSlateBrush* IToolShutdownOverlayWidget::GetToolIcon(const TWeakPtr<FModeToolkit>& InWeakToolkit)
{
	FString ActiveToolIdentifier = InWeakToolkit.Pin()->GetScriptableEditorMode()->GetToolManager(EToolsContextScope::EdMode)->GetActiveToolName(EToolSide::Left);
	ActiveToolIdentifier.InsertAt(0, ".");
	FName ActiveToolIconName = ISlateStyle::Join(FRoadEditorCommands::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	const FSlateBrush* ActiveToolIcon = FMetaRoadEditorStyle::Get().GetOptionalBrush(ActiveToolIconName);
	return ActiveToolIcon;
}

TSharedRef<SWidget> IToolShutdownOverlayWidgetDefault::MakeShutdownOverlayWidget(const TWeakPtr<FModeToolkit>& InWeakToolkit)
{
	WeakToolkit = InWeakToolkit;

	// All lambdas below are Slate attribute getters polled every frame. They must NOT
	// dereference `this` after the UObject has been GC'd (which can happen when a new
	// tool activates and the previous shutdown overlay widget is still alive).
	// Guard every access through WeakSelf: if the UObject is gone, return a safe default.
	TWeakObjectPtr<UObject> WeakSelf(Cast<UObject>(this));

	// Helpers to determine button/label visibility based on overrides
	auto GetSubActionIcon = [WeakSelf, this]() -> const FSlateBrush*
	{
		if (!WeakSelf.IsValid()) return nullptr;
		if (AcceptCancelButtonParams.IsSet())
		{
			if (AcceptCancelButtonParams->IconName.IsSet())
			{
				return FModelingToolsEditorModeStyle::Get()->GetOptionalBrush(AcceptCancelButtonParams->IconName.GetValue(), nullptr, nullptr);
			}
		}
		else if (CompleteButtonParams.IsSet() && CompleteButtonParams->IconName.IsSet())
		{
			return FModelingToolsEditorModeStyle::Get()->GetOptionalBrush(CompleteButtonParams->IconName.GetValue(), nullptr, nullptr);
		}
		return nullptr;
	};
	auto GetSubActionIconVisibility = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return EVisibility::Collapsed;
		if (AcceptCancelButtonParams.IsSet())
		{
			if (AcceptCancelButtonParams->IconName.IsSet()
				&& FModelingToolsEditorModeStyle::Get()->GetOptionalBrush(AcceptCancelButtonParams->IconName.GetValue(), nullptr, nullptr))
			{
				return EVisibility::Visible;
			}
		}
		else if (CompleteButtonParams.IsSet() && CompleteButtonParams->IconName.IsSet()
			&& FModelingToolsEditorModeStyle::Get()->GetOptionalBrush(CompleteButtonParams->IconName.GetValue(), nullptr, nullptr))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	};
	auto GetSubActionLabel = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return AcceptCancelButtonParams.IsSet() ? AcceptCancelButtonParams->Label
			: CompleteButtonParams.IsSet() ? CompleteButtonParams->Label
			: FText::GetEmpty();
	};
	auto GetSubActionLabelVisibility = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return EVisibility::Collapsed;
		return (AcceptCancelButtonParams.IsSet() || CompleteButtonParams.IsSet()) ?
			EVisibility::Visible : EVisibility::Collapsed;
	};
	auto GetAcceptButtonText = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return AcceptCancelButtonParams.IsSet() && AcceptCancelButtonParams->OverrideAcceptButtonText.IsSet() ?
			AcceptCancelButtonParams->OverrideAcceptButtonText.GetValue() : LOCTEXT("OverlayAccept", "Accept");
	};
	auto GetAcceptButtonTooltip = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return AcceptCancelButtonParams.IsSet() && AcceptCancelButtonParams->OverrideAcceptButtonTooltip.IsSet() ?
			AcceptCancelButtonParams->OverrideAcceptButtonTooltip.GetValue()
			: LOCTEXT("OverlayAcceptTooltip", "Accept/Commit the results of the active Tool [Enter]");
	};
	auto GetAcceptButtonEnabled = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return false;
		return AcceptCancelButtonParams.IsSet() ? AcceptCancelButtonParams->CanAccept()
			: WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanAcceptActiveTool();
	};
	auto GetAcceptCancelButtonVisibility = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return EVisibility::Collapsed;
		if (AcceptCancelButtonParams.IsSet()
			|| (!CompleteButtonParams.IsSet()
				&& WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->ActiveToolHasAccept()))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	};
	auto GetCancelButtonText = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return AcceptCancelButtonParams.IsSet() && AcceptCancelButtonParams->OverrideCancelButtonText.IsSet() ?
			AcceptCancelButtonParams->OverrideCancelButtonText.GetValue() : LOCTEXT("OverlayCancel", "Cancel");
	};
	auto GetCancelButtonTooltip = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return AcceptCancelButtonParams.IsSet() && AcceptCancelButtonParams->OverrideCancelButtonTooltip.IsSet() ?
			AcceptCancelButtonParams->OverrideCancelButtonTooltip.GetValue()
			: LOCTEXT("OverlayCancelTooltip", "Cancel the active Tool [Esc]");
	};
	auto GetCancelButtonEnabled = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return false;
		return AcceptCancelButtonParams.IsSet()
			|| WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCancelActiveTool();
	};
	auto GetCompleteButtonText = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return CompleteButtonParams.IsSet() && CompleteButtonParams->OverrideCompleteButtonText.IsSet() ?
			CompleteButtonParams->OverrideCompleteButtonText.GetValue() : LOCTEXT("OverlayComplete", "Complete");
	};
	auto GetCompleteButtonTooltip = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return FText::GetEmpty();
		return CompleteButtonParams.IsSet() && CompleteButtonParams->OverrideCompleteButtonTooltip.IsSet() ?
			CompleteButtonParams->OverrideCompleteButtonTooltip.GetValue()
			: LOCTEXT("OverlayCompleteTooltip", "Exit the active Tool [Enter]");
	};
	auto GetCompleteButtonEnabled = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return false;
		return CompleteButtonParams.IsSet()
			|| WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCompleteActiveTool();
	};
	auto GetCompleteButtonVisibility = [WeakSelf, this]()
	{
		if (!WeakSelf.IsValid()) return EVisibility::Collapsed;
		if (CompleteButtonParams.IsSet()
			|| (!AcceptCancelButtonParams.IsSet()
				&& WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->CanCompleteActiveTool()))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	};

	auto ToolShutdownViewportOverlayWidget = SNew(SHorizontalBox)

	+SHorizontalBox::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)

			// Tool icon and name
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda([WeakSelf, this, InWeakToolkit] () -> const FSlateBrush*
				{
					return WeakSelf.IsValid() ? GetToolIcon(InWeakToolkit) : nullptr;
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(WeakToolkit.Pin()->GetActiveToolDisplayName())
			]

			// Optional: "-> [icon] SubtoolAction"
			// arrow
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(FMargin(0., 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image(FModelingToolsEditorModeStyle::Get()->GetBrush("ModelingMode.SubToolArrow"))
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Visibility_Lambda(GetSubActionLabelVisibility)
			]
			// subaction icon
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image_Lambda(GetSubActionIcon)
				.Visibility_Lambda(GetSubActionIconVisibility)
			]
			// subaction label
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda(GetSubActionLabel)
				.Visibility_Lambda(GetSubActionLabelVisibility)
			]

			// Buttons:
			// Accept
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0, 0.f, 2.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text_Lambda(GetAcceptButtonText)
				.ToolTipText_Lambda(GetAcceptButtonTooltip)
				.OnClicked_Raw(this, &IToolShutdownOverlayWidgetDefault::HandleAcceptCancelClick, true)
				.IsEnabled_Lambda(GetAcceptButtonEnabled)
				.Visibility_Lambda(GetAcceptCancelButtonVisibility)
			]
			// Cancel
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.Text_Lambda(GetCancelButtonText)
				.ToolTipText_Lambda(GetCancelButtonTooltip)
				.HAlign(HAlign_Center)
				.OnClicked_Raw(this, &IToolShutdownOverlayWidgetDefault::HandleAcceptCancelClick, false)
				.IsEnabled_Lambda(GetCancelButtonEnabled)
				.Visibility_Lambda(GetAcceptCancelButtonVisibility)
			]
			// Complete
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SPrimaryButton)
				.Text_Lambda(GetCompleteButtonText)
				.ToolTipText_Lambda(GetCompleteButtonTooltip)
				.OnClicked_Raw(this, &IToolShutdownOverlayWidgetDefault::HandleCompleteClick)
				.IsEnabled_Lambda(GetCompleteButtonEnabled)
				.Visibility_Lambda(GetCompleteButtonVisibility)
			]
		]	
	];

	return ToolShutdownViewportOverlayWidget;
}

/*
bool IToolShutdownOverlayWidgetDefault::RequestAcceptCancelButtonOverride(IToolHostCustomizationAPI::FAcceptCancelButtonOverrideParams& Params)
{
	if (!Params.OnAcceptCancelTriggered || !Params.CanAccept || Params.Label.IsEmpty())
	{
		UE_LOG(LogGeometry, Warning, TEXT("FMetaRoadEditorModeToolkit::RequestAcceptCancelButtonOverride received request with insufficient parameters."));
		return false;
	}

	AcceptCancelButtonParams = Params;
	CompleteButtonParams.Reset();
	bCurrentOverrideButtonsWereClicked = false;

	if (ToolShutdownViewportOverlayWidget)
	{
		ToolShutdownViewportOverlayWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	return true;
}

bool IToolShutdownOverlayWidgetDefault::RequestCompleteButtonOverride(IToolHostCustomizationAPI::FCompleteButtonOverrideParams& Params)
{
	if (!Params.OnCompleteTriggered || Params.Label.IsEmpty())
	{
		UE_LOG(LogGeometry, Warning, TEXT("FMetaRoadEditorModeToolkit::RequestCompleteButtonOverride received request with insufficient parameters."));
		return false;
	}

	CompleteButtonParams = Params;
	AcceptCancelButtonParams.Reset();
	bCurrentOverrideButtonsWereClicked = false;

	if (ToolShutdownViewportOverlayWidget)
	{
		ToolShutdownViewportOverlayWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
	return true;
}

void IToolShutdownOverlayWidgetDefault::ClearButtonOverrides()
{
	AcceptCancelButtonParams.Reset();
	CompleteButtonParams.Reset();
	if (ToolShutdownViewportOverlayWidget)
	{
		ToolShutdownViewportOverlayWidget->Invalidate(EInvalidateWidgetReason::Layout);
	}
}
*/


FReply IToolShutdownOverlayWidgetDefault::HandleAcceptCancelClick(bool bAccept)
{
	if (AcceptCancelButtonParams.IsSet())
	{
		bCurrentOverrideButtonsWereClicked = true;
		if (ensure(AcceptCancelButtonParams->OnAcceptCancelTriggered))
		{
			AcceptCancelButtonParams->OnAcceptCancelTriggered(bAccept);
		}

		// This will be reset back to false if the callback above triggers another override request.
		if (bCurrentOverrideButtonsWereClicked)
		{
			//ClearButtonOverrides();
		}
	}
	else
	{
		WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(bAccept ? EToolShutdownType::Accept : EToolShutdownType::Cancel);
	}
	return FReply::Handled();
}
FReply IToolShutdownOverlayWidgetDefault::HandleCompleteClick()
{
	if (CompleteButtonParams.IsSet())
	{
		bCurrentOverrideButtonsWereClicked = true;
		if (ensure(CompleteButtonParams->OnCompleteTriggered))
		{
			CompleteButtonParams->OnCompleteTriggered();
		}

		// This will be reset back to false if the callback above triggers another override request.
		if (bCurrentOverrideButtonsWereClicked)
		{
			//ClearButtonOverrides();
		}
	}
	else
	{
		WeakToolkit.Pin()->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode)->EndTool(EToolShutdownType::Completed);
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE