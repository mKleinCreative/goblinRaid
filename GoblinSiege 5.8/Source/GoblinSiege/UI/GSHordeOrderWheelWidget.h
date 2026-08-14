// The order wheel's on-screen half. Written 2026-08-12, ticket #141. Sibling of
// UGSWeaponWheelWidget, and deliberately the same shape: a plain UUserWidget that binds two
// component delegates and recolours labels.
//
// The widget OWNS NOTHING. Every sector boundary comes from
// UGSHordeCommandComponent::OrderForDirection, and every colour from
// AGSHordeOrderMarker::ColourForOrder - so the sector you see lit is the sector the input will
// choose, and the colour it lights in is the colour the beacon will be on the ground. Two copies of
// either mapping is how a menu starts lying about what the world will do.
//
// NOT a UCommonActivatableWidget. CommonUI is enabled and linked in GoblinSiege.Build.cs but has
// never been used by a single class in this project; being the first would introduce a second UI
// paradigm alongside the plain-UUserWidget one for a panel with five text blocks on it.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Horde/GSHordeCommandComponent.h"   // EGSHordeOrder, and the delegate signatures
#include "GSHordeOrderWheelWidget.generated.h"

class UTextBlock;

UCLASS()
class GOBLINSIEGE_API UGSHordeOrderWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Re-reads the component and repaints. Safe at any time. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Wheel")
	void RefreshFromCommandComponent();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * BindWidgetOptional on all five, for UGSWeaponWheelWidget's reason: WBP_HordeOrderWheel is
	 * authored content, and a hard BindWidget would turn a designer renaming a label into a Blueprint
	 * compile error pointing at C++. Optional degrades to "that label does not light up", and
	 * NativeConstruct says so once.
	 *
	 * BlueprintReadOnly is NOT decoration and must not be dropped. A BindWidgetOptional property that
	 * any Blueprint graph reads must also be BlueprintReadOnly or the whole widget BP fails to
	 * compile with "is not blueprint visible" - which is exactly how the HUD's stamina bar froze in
	 * #064. The weapon wheel gets away without it only because nothing reads its labels from a graph.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<UTextBlock> Label_Attack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<UTextBlock> Label_Hold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<UTextBlock> Label_Follow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<UTextBlock> Label_Loot;

	/**
	 * What the order is about to be aimed at, written on open.
	 *
	 * This is the entire visible payoff of latching. Without it the player has no way to tell whether
	 * the game caught the guard he was looking at or the fence behind him, and "Attack" becomes a
	 * gesture he has to make and then watch to find out what it meant.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|Horde|Wheel")
	TObjectPtr<UTextBlock> Label_Subject;

	/** Dimming applied to the highlighted verb while the drag is still inside the dead zone, so
	 *  "drag back to centre cancels" is visible rather than something you have to know. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UncommittedAlpha = 0.6f;

	/**
	 * Everything not currently pointed at. Dimmed rather than hidden, so the wheel still reads as
	 * four choices.
	 *
	 * RAISED 0.45 -> 0.85 (#150). At 0.45 the labels were legible against the screen corner they
	 * originally sat in and effectively invisible once #146 moved them to screen centre over a bright
	 * arena - Michael reported the wheel had stopped appearing at all. It had not: the live widget
	 * was confirmed in the viewport, HitTestInvisible, with all four labels at 45% white. A UI element
	 * that only reads against a dark background is not dim, it is broken; the black shadow added to
	 * the labels in the same pass is the other half of the fix.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel|Style")
	FLinearColor IdleColour = FLinearColor(1.f, 1.f, 1.f, 0.85f);

	/**
	 * Attack and Loot when nothing orderable is latched - i.e. the two verbs the subsystem will
	 * refuse outright.
	 *
	 * Added after Michael watched the first build and said "there's no way to give it anything to
	 * attack". Two thirds of that was the trace, now fixed; this is the remaining third. Without it
	 * the wheel looks identical whether you are pointing at a guard or at grass, so the player finds
	 * out his order was meaningless by watching nothing happen. Greying the verb out says it up front.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Wheel|Style")
	FLinearColor UnavailableColour = FLinearColor(0.45f, 0.45f, 0.5f, 0.55f);

	UFUNCTION()
	void HandleOrderWheelOpenChanged(bool bOpen);

	UFUNCTION()
	void HandleOrderWheelHighlightChanged(EGSHordeOrder Highlighted);

private:
	/** Weak: the widget outlives nothing, but the raid director can destroy and respawn the pawn. */
	TWeakObjectPtr<UGSHordeCommandComponent> BoundComponent;

	void Repaint(EGSHordeOrder Highlighted, bool bCommitted);

	/** WhichOrder, not Order - and never `Slot`. UWidget::Slot exists, this module builds
	 *  warnings-as-errors, and shadowing it is a hard compile failure (see #047). */
	UTextBlock* LabelFor(EGSHordeOrder WhichOrder) const;
};
