// Captain/Engineer-style support aura: periodically applies a GameplayEffect to nearby same-team
// pawns ("buffs nearby militia" - race-design-humans.md Captain; Engineer's Bunker heal reuses
// this with a different GE). Priority-kill gameplay: kill the carrier to break the aura.
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBuffAuraComponent.generated.h"

class UGameplayEffect;

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSBuffAuraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBuffAuraComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void AuraTick();

	/** Duration-based GE (e.g. +attack speed for 1.5s) - reapplication refreshes it, so leaving
	 *  the radius lets it lapse naturally. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Aura")
	TSubclassOf<UGameplayEffect> AuraEffectClass;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Aura")
	float AuraRadius = 600.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Aura")
	float ReapplyIntervalSeconds = 1.f;

	FTimerHandle AuraTimerHandle;
};
