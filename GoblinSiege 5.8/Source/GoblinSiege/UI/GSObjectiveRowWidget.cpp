#include "UI/GSObjectiveRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGSObjectiveRowWidget::SetRow(const FGSObjectiveDisplayRow& Row, UTexture2D* TypeTexture, UTexture2D* StateTexture)
{
	const bool bDone = Row.Icon == EGSObjectiveRowIcon::Done;
	const FLinearColor Tint = bDone ? DoneTint : ActiveTint;

	if (LabelText)
	{
		LabelText->SetText(Row.Label);
		LabelText->SetColorAndOpacity(FSlateColor(Tint));
	}

	if (DetailText)
	{
		DetailText->SetText(Row.Detail);
		DetailText->SetColorAndOpacity(FSlateColor(Tint));
		// Collapsed rather than hidden: a blank qualifier should not reserve the space it would
		// have taken, or every unqualified row sits next to an empty gap.
		DetailText->SetVisibility(Row.Detail.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// A null texture collapses its image instead of drawing Slate's white placeholder square, which
	// reads as a bug rather than as missing art.
	auto Apply = [](UImage* Image, UTexture2D* Texture, const FLinearColor& InTint)
	{
		if (!Image)
		{
			return;
		}
		if (!Texture)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetBrushFromTexture(Texture, false);
		Image->SetColorAndOpacity(InTint);
	};

	Apply(TypeIcon, TypeTexture, Tint);
	// The state marker keeps its own colour: the flame is meant to look hot and the charred X is
	// meant to look dead, and dimming them to match the text would throw away the one part of the
	// row that is readable at a glance.
	Apply(StateIcon, StateTexture, FLinearColor::White);
}
