// The GameplayEffect that fire damage rides on. Written 2026-07-30.
//
// AGSFireVolume::DamageTick() shipped as an empty TODO, so fire in this project has never actually
// hurt anyone - not the field, not the torch. The missing piece was a GameplayEffect carrying
// UGSDamageExecCalculation; everything else (attribute set, IncomingDamage meta attribute drained
// into Health, race matchup matrix, armor mitigation) was already built and working.
//
// Configured in the constructor rather than as a content asset on purpose: the damage numbers are
// SetByCaller (per-hazard tuning lives on the hazard), and the only thing this class contributes is
// "run the shared damage exec calc, instantly". Nothing here is worth an asset a designer could
// silently break, and a C++ default class can't go missing from a content folder.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSGE_FireDamage.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGE_FireDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSGE_FireDamage();
};
