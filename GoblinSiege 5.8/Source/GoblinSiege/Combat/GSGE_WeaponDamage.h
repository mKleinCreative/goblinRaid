// Instant damage GameplayEffect for weapon hits (melee now, ranged later).
//
// Deliberately a sibling of UGSGE_FireDamage rather than a reuse of it. Both are one-line
// classes whose only job is to carry UGSDamageExecCalculation, and neither bakes a Damage.*
// tag of its own - the attacking ability supplies both the tag and the SetByCaller magnitude
// on the spec. Reusing the fire effect for a sword would have worked identically and read as
// a bug forever after.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSGE_WeaponDamage.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGE_WeaponDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSGE_WeaponDamage();
};
