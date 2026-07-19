// The single damage pipeline (design doc §5-6): every attack applies one GameplayEffect whose
// execution runs this calc - read the Damage.* tag + SetByCaller magnitude, multiply by the
// target's race-matchup row (GSRaceDataAsset), apply flat armor unless blast/armor-ignoring,
// write the result to the IncomingDamage meta attribute. Reconstructed 2026-07-19 to match the
// surviving GSDamageExecCalculation.cpp.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GSDamageExecCalculation.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSDamageExecCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UGSDamageExecCalculation();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
