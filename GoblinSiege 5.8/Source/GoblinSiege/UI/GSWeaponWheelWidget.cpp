#include "UI/GSWeaponWheelWidget.h"
#include "Combat/GSGameplayTags.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UGSWeaponWheelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// FindComponentByClass on the owning pawn rather than a cast to AGSPlayerCharacter, matching the
	// reasoning every other consumer of this component uses: the wheel is a property of carrying a
	// weapon, not of being the player class.
	if (const APawn* Owner = GetOwningPlayerPawn())
	{
		BoundComponent = Owner->FindComponentByClass<UGSWeaponComponent>();
	}

	if (UGSWeaponComponent* Comp = BoundComponent.Get())
	{
		Comp->OnWheelOpenChanged.AddDynamic(this, &UGSWeaponWheelWidget::HandleWheelOpenChanged);
		Comp->OnWheelHighlightChanged.AddDynamic(this, &UGSWeaponWheelWidget::HandleWheelHighlightChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s found no UGSWeaponComponent on its owning pawn - the wheel will "
				 "never appear. The widget was probably created before the pawn was possessed."),
			*GetName());
	}

	// Missing labels are survivable, but silence about them is not: a renamed widget in
	// WBP_WeaponWheel produces a wheel that opens and highlights nothing, which looks like the input
	// is broken rather than the binding.
	if (!Label_Torch || !Label_Bow || !Label_Sword)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s is missing one or more of Label_Torch / Label_Bow / Label_Sword. "
				 "Those exact names are the BindWidgetOptional contract; a renamed label simply will "
				 "not light up."),
			*GetName());
	}

	// Hidden until the wheel opens. Set here rather than trusting the asset's saved visibility, which
	// a designer can change in the editor without realising it is load-bearing.
	SetVisibility(ESlateVisibility::Collapsed);
	RefreshFromWeaponComponent();
}

void UGSWeaponWheelWidget::NativeDestruct()
{
	// Unbind explicitly. The pawn outliving this widget is the normal case on a level transition, and
	// a stale dynamic delegate on a destroyed widget is a crash rather than a leak.
	if (UGSWeaponComponent* Comp = BoundComponent.Get())
	{
		Comp->OnWheelOpenChanged.RemoveDynamic(this, &UGSWeaponWheelWidget::HandleWheelOpenChanged);
		Comp->OnWheelHighlightChanged.RemoveDynamic(this, &UGSWeaponWheelWidget::HandleWheelHighlightChanged);
	}
	BoundComponent.Reset();

	Super::NativeDestruct();
}

void UGSWeaponWheelWidget::RefreshFromWeaponComponent()
{
	if (const UGSWeaponComponent* Comp = BoundComponent.Get())
	{
		Repaint(Comp->GetWheelHighlight(), Comp->IsWheelCommitted());
	}
}

void UGSWeaponWheelWidget::HandleWheelOpenChanged(bool bOpen)
{
	// HitTestInvisible, not Visible: the wheel is driven by accumulated mouse delta through
	// Input_Look, and a hit-testable panel over the viewport would swallow the very input it exists
	// to visualise.
	SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bOpen)
	{
		// Paint the opening state immediately. OnWheelHighlightChanged fires only on CHANGE, so a
		// wheel opened on the slot you are already holding would otherwise appear with last time's
		// colours until the player moved the mouse.
		RefreshFromWeaponComponent();
	}
}

void UGSWeaponWheelWidget::HandleWheelHighlightChanged(FGameplayTag Highlighted)
{
	const UGSWeaponComponent* Comp = BoundComponent.Get();
	Repaint(Highlighted, Comp && Comp->IsWheelCommitted());
}

// Parameter is WhichSlot, not Slot: UWidget already has a `Slot` member (its UPanelSlot), and this
// module builds warnings-as-errors, so shadowing it is a hard compile failure rather than a hint.
UTextBlock* UGSWeaponWheelWidget::LabelFor(FGameplayTag WhichSlot) const
{
	// A tag cannot be switched on, so this is a chain. It is still exhaustive over the four labels
	// this widget owns, and an unknown slot returns null rather than falling through to Sword -
	// lighting the wrong label would be a worse lie than lighting none.
	if (WhichSlot == GSTags::WeaponSlot_Torch)   { return Label_Torch; }
	if (WhichSlot == GSTags::WeaponSlot_Bow)     { return Label_Bow; }
	if (WhichSlot == GSTags::WeaponSlot_Sword)   { return Label_Sword; }
	if (WhichSlot == GSTags::WeaponSlot_Grapple) { return Label_Grapple; }
	return nullptr;
}

void UGSWeaponWheelWidget::Repaint(FGameplayTag Highlighted, bool bCommitted)
{
	// Four labels, one lit. Written as a loop over the enum rather than explicit assignments so
	// adding a slot cannot leave one label stuck on the previous frame's colour - which is exactly
	// what this array being kept in step bought when Grapple landed (2026-08-17).
	const FGameplayTag All[] = { GSTags::WeaponSlot_Torch, GSTags::WeaponSlot_Bow,
								 GSTags::WeaponSlot_Sword, GSTags::WeaponSlot_Grapple };
	for (const FGameplayTag& WhichSlot : All)
	{
		if (UTextBlock* Label = LabelFor(WhichSlot))
		{
			const bool bIsHighlighted = (WhichSlot == Highlighted);
			const FLinearColor Colour = bIsHighlighted
				? (bCommitted ? HighlightColour : UncommittedColour)
				: IdleColour;
			Label->SetColorAndOpacity(FSlateColor(Colour));
		}
	}
}
