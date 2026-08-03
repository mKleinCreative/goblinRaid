// Native parent for WBP_GSPlayerHUD.
//
// The widget already existed and already had a HealthBar and a HealthText - but the health caption
// was the LITERAL string "HP  100 / 100" with no format tokens and no GetHealth call anywhere in
// the package, so it has been decorative since the day it was made. That is why the bar never moved
// during the fire test.
//
// Driving it from C++ rather than from a UMG property binding, for three reasons: the Health
// attribute's change delegate in GAS is non-dynamic and cannot be bound from Blueprint at all, so
// the alternative is polling every frame in the widget graph; BindWidget resolves the two widgets
// by name with no wiring to forget; and a compile error is a better failure than a health bar that
// silently stops updating.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GSPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class AGSCharacterBase;

UCLASS()
class GOBLINSIEGE_API UGSPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Point this at a character to follow. Called automatically for the owning pawn; exposed so a
	 *  spectator or a boss bar can retarget it. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|HUD")
	void BindToCharacter(AGSCharacterBase* Character);

protected:
	/** BindWidgetOptional, not BindWidget: the names must match what is already in the .uasset, and
	 *  a hard BindWidget would refuse to compile the Blueprint if either were ever renamed - which
	 *  turns a cosmetic problem into a broken HUD. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	/** Bound to AGSCharacterBase::OnHealthChanged. */
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth, float Delta);

	/** Called on damage so a Blueprint can flash the bar, shake, play a sound - anything that wants
	 *  to react to a hit without duplicating the binding. Delta is negative. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnDamaged(float Delta, float NewHealth, float MaxHealth);

	void Refresh(float NewHealth, float MaxHealth);

private:
	UPROPERTY()
	TWeakObjectPtr<AGSCharacterBase> BoundCharacter;
};
