#include "UI/GSPlayerHUDWidget.h"
#include "Characters/GSCharacterBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UGSPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// GetOwningPlayerPawn can legitimately be null on the first construct if the widget is created
	// before possession; BindToCharacter is public so whoever creates the widget can retry.
	BindToCharacter(Cast<AGSCharacterBase>(GetOwningPlayerPawn()));
}

void UGSPlayerHUDWidget::NativeDestruct()
{
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnHealthChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);
	}
	Super::NativeDestruct();
}

void UGSPlayerHUDWidget::BindToCharacter(AGSCharacterBase* Character)
{
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnHealthChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);
	}

	BoundCharacter = Character;

	if (!Character)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Player HUD has no character to follow - the health bar will not move."));
		return;
	}

	Character->OnHealthChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);

	// Paint the current value immediately. Without this the bar shows whatever the designer left in
	// the .uasset until the first point of damage, which is exactly the bug this class exists to
	// fix - it just moves the lie from "always" to "until you get hit".
	Refresh(Character->GetHealth(), Character->GetMaxHealth());
}

void UGSPlayerHUDWidget::HandleHealthChanged(float NewHealth, float MaxHealth, float Delta)
{
	Refresh(NewHealth, MaxHealth);

	if (Delta < 0.f)
	{
		OnDamaged(Delta, NewHealth, MaxHealth);
	}
}

void UGSPlayerHUDWidget::Refresh(float NewHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0.f ? FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f) : 0.f);
	}

	if (HealthText)
	{
		// CeilToInt, not RoundToInt: 0.4 health remaining should read as 1, not as 0. A player
		// staring at "HP 0 / 100" while still alive will report it as a bug, and be right to.
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %d / %d"),
			FMath::CeilToInt(FMath::Max(0.f, NewHealth)), FMath::CeilToInt(MaxHealth))));
	}
}
