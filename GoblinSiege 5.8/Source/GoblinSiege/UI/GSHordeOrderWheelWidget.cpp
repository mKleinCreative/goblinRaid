#include "UI/GSHordeOrderWheelWidget.h"

#include "Horde/GSHordeOrderMarker.h"   // ColourForOrder - one colour table, not two
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UGSHordeOrderWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// FindComponentByClass on the owning pawn rather than a cast to AGSPlayerCharacter, matching
	// UGSWeaponWheelWidget: commanding the horde is a property of having summoned it, not of being
	// the player class.
	if (const APawn* Owner = GetOwningPlayerPawn())
	{
		BoundComponent = Owner->FindComponentByClass<UGSHordeCommandComponent>();
	}

	if (UGSHordeCommandComponent* Comp = BoundComponent.Get())
	{
		Comp->OnOrderWheelOpenChanged.AddDynamic(this, &UGSHordeOrderWheelWidget::HandleOrderWheelOpenChanged);
		Comp->OnOrderWheelHighlightChanged.AddDynamic(this, &UGSHordeOrderWheelWidget::HandleOrderWheelHighlightChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s found no UGSHordeCommandComponent on its owning pawn - the order "
			     "wheel will never appear. The widget was probably created before the pawn was "
			     "possessed."), *GetName());
	}

	if (!Label_Attack || !Label_Hold || !Label_Follow || !Label_Loot)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s is missing one or more of Label_Attack / Label_Hold / Label_Follow "
			     "/ Label_Loot. Those exact names are the BindWidgetOptional contract; a renamed label "
			     "simply will not light up."), *GetName());
	}

	// Hidden until the wheel opens. Set here rather than trusting the asset's saved visibility, which
	// a designer can change without realising it is load-bearing.
	SetVisibility(ESlateVisibility::Collapsed);
	RefreshFromCommandComponent();
}

void UGSHordeOrderWheelWidget::NativeDestruct()
{
	// Unbind explicitly. The pawn outliving this widget is the normal case on a level transition, and
	// a stale dynamic delegate on a destroyed widget is a crash rather than a leak.
	if (UGSHordeCommandComponent* Comp = BoundComponent.Get())
	{
		Comp->OnOrderWheelOpenChanged.RemoveDynamic(this, &UGSHordeOrderWheelWidget::HandleOrderWheelOpenChanged);
		Comp->OnOrderWheelHighlightChanged.RemoveDynamic(this, &UGSHordeOrderWheelWidget::HandleOrderWheelHighlightChanged);
	}
	BoundComponent.Reset();

	Super::NativeDestruct();
}

void UGSHordeOrderWheelWidget::RefreshFromCommandComponent()
{
	const UGSHordeCommandComponent* Comp = BoundComponent.Get();
	if (!Comp)
	{
		return;
	}

	if (Label_Subject)
	{
		Label_Subject->SetText(Comp->GetLatchedSubjectLabel());
	}

	Repaint(Comp->GetWheelHighlight(), Comp->IsWheelCommitted());
}

void UGSHordeOrderWheelWidget::HandleOrderWheelOpenChanged(bool bOpen)
{
	// HitTestInvisible, not Visible: the wheel is driven by accumulated mouse delta routed through
	// Input_Look, and a hit-testable panel over the viewport would swallow the very input it exists
	// to visualise.
	SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bOpen)
	{
		// Paint the opening state immediately, and in particular write the subject label: the
		// highlight delegate fires only on CHANGE, and the wheel opens on None having changed
		// nothing, so without this the panel would appear showing the last gesture's target.
		RefreshFromCommandComponent();
	}
}

void UGSHordeOrderWheelWidget::HandleOrderWheelHighlightChanged(EGSHordeOrder Highlighted)
{
	const UGSHordeCommandComponent* Comp = BoundComponent.Get();
	Repaint(Highlighted, Comp && Comp->IsWheelCommitted());
}

UTextBlock* UGSHordeOrderWheelWidget::LabelFor(EGSHordeOrder WhichOrder) const
{
	switch (WhichOrder)
	{
	case EGSHordeOrder::Attack: return Label_Attack;
	case EGSHordeOrder::Hold:   return Label_Hold;
	case EGSHordeOrder::Follow: return Label_Follow;
	case EGSHordeOrder::Loot:   return Label_Loot;
	default:                    return nullptr;   // None owns no label - it IS the centre
	}
}

void UGSHordeOrderWheelWidget::Repaint(EGSHordeOrder Highlighted, bool bCommitted)
{
	// Written as a loop over the verbs rather than four explicit assignments, so adding a fifth
	// cannot leave one label stuck on the previous frame's colour.
	const EGSHordeOrder All[] =
	{
		EGSHordeOrder::Attack,
		EGSHordeOrder::Hold,
		EGSHordeOrder::Follow,
		EGSHordeOrder::Loot
	};

	// Whether the two subject-hungry verbs are actually issuable right now. Mirrors the refusal in
	// UGSHordeSubsystem::IssueOrder rather than guessing - if that ever stops refusing, this stops
	// greying out, because both are asking the same question about the same latch.
	const UGSHordeCommandComponent* Comp = BoundComponent.Get();
	const bool bHasSubject = Comp && Comp->GetLatchedSubject() != nullptr;

	for (const EGSHordeOrder WhichOrder : All)
	{
		UTextBlock* Label = LabelFor(WhichOrder);
		if (!Label)
		{
			continue;
		}

		const bool bNeedsSubject =
			(WhichOrder == EGSHordeOrder::Attack || WhichOrder == EGSHordeOrder::Loot);

		if (bNeedsSubject && !bHasSubject)
		{
			// Greyed whether or not it is the one being pointed at. Highlighting a verb that will be
			// refused on release is precisely the lie this exists to stop.
			Label->SetColorAndOpacity(FSlateColor(UnavailableColour));
			continue;
		}

		if (WhichOrder != Highlighted)
		{
			Label->SetColorAndOpacity(FSlateColor(IdleColour));
			continue;
		}

		// The verb lights in the colour its beacon will be. Not a style choice made here - it is
		// AGSHordeOrderMarker's table, so the two cannot drift.
		FLinearColor Colour = AGSHordeOrderMarker::ColourForOrder(WhichOrder);
		if (!bCommitted)
		{
			// Still inside the dead zone. Without this the wheel looks identical at rest and at
			// commit, so cancelling by dragging back to centre would be invisible.
			Colour.A *= UncommittedAlpha;
		}
		Label->SetColorAndOpacity(FSlateColor(Colour));
	}
}
