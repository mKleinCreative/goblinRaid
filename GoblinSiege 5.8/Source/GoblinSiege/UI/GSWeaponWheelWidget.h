// The radial weapon wheel's on-screen half. Added 2026-08-06, after #039-#041 shipped a wheel that
// worked and could not be seen - Q selected correctly, but the player dragged blind and only learned
// what they had picked by looking at their hands afterwards.
//
// Deliberately a C++ UUserWidget rather than a Blueprint graph. The alternative was authoring the
// delegate binding node-by-node through VibeUE's BlueprintService, which can do it but takes dozens
// of pin-level calls whose failure mode is a silently broken graph. This binds two delegates and
// recolours three labels; that is not worth a graph, and BindWidget makes the widget names a
// compile-checked contract instead of a naming convention nobody enforces.
//
// The widget OWNS NOTHING. It reads state from UGSWeaponComponent and draws it. All selection maths
// lives in SlotForDirection - the widget must never re-derive which sector is which, because two
// copies of that mapping is exactly how a preview starts lying about what the input will do.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapons/GSWeaponComponent.h"   // the delegate signatures
#include "GameplayTagContainer.h"
#include "GSWeaponWheelWidget.generated.h"

class UTextBlock;

UCLASS()
class GOBLINSIEGE_API UGSWeaponWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Re-reads the component and repaints. Safe to call at any time; used by the character when the
	 *  widget is created after the pawn already has a slot selected. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Wheel")
	void RefreshFromWeaponComponent();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * BindWidgetOptional, not BindWidget, on all three.
	 *
	 * BindWidget makes a missing widget a COMPILE ERROR on the Blueprint - which sounds like the
	 * safer choice and is not: WBP_WeaponWheel is authored content, and a designer renaming a label
	 * would break the asset with an error that points at C++. Optional binding degrades to "that
	 * label does not highlight", and NativeConstruct says so once in the log.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label_Torch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label_Bow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label_Primary;

	/** The fourth slot (2026-08-17). BindWidgetOptional like the others, so a WBP that has not been
	 *  re-authored yet still compiles and simply shows no grapple label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label_Grapple;

	/** The slot the drag is currently pointing at. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Wheel|Style")
	FLinearColor HighlightColour = FLinearColor(1.f, 0.82f, 0.25f, 1.f);

	/** Everything else. Dimmed rather than hidden, so the wheel still reads as three choices. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Wheel|Style")
	FLinearColor IdleColour = FLinearColor(1.f, 1.f, 1.f, 0.45f);

	/**
	 * Applied to the highlighted label while the drag is still inside the dead zone.
	 *
	 * Without this the wheel looks identical at rest and at commit, so "drag back to centre cancels"
	 * would be invisible - the player would see their current slot lit and reasonably assume
	 * releasing selects it. It does, but only because it is already selected, which is a different
	 * thing and reads as a bug the first time someone tries to cancel.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Wheel|Style")
	FLinearColor UncommittedColour = FLinearColor(1.f, 1.f, 1.f, 0.75f);

	UFUNCTION()
	void HandleWheelOpenChanged(bool bOpen);

	UFUNCTION()
	void HandleWheelHighlightChanged(FGameplayTag Highlighted);

private:
	/** The component this widget is bound to. Weak: the widget outlives nothing, but a pawn can be
	 *  destroyed and respawned by the raid director underneath it. */
	TWeakObjectPtr<UGSWeaponComponent> BoundComponent;

	void Repaint(FGameplayTag Highlighted, bool bCommitted);
	/** WhichSlot, not Slot - UWidget::Slot exists and shadowing it is a build error here. */
	UTextBlock* LabelFor(FGameplayTag WhichSlot) const;
};
