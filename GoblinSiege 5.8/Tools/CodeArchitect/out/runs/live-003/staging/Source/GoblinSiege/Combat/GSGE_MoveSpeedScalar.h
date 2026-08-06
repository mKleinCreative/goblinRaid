// One infinite GameplayEffect behind every "you move slower while X" rule - carry state now, block,
// and the roots/slows the stealth five will want later. The scalar arrives as a SetByCaller
// magnitude, so one class serves every source instead of a GE per source; same reasoning that keeps
// UGSGE_WeaponDamage free of a baked Damage.* tag.
//
// This is what UGSAttributeSetBase::MoveSpeedMultiplier was declared for ("lets abilities/effects
// stack cleanly") and never wired to. Cache-and-restore on MaxWalkSpeed cannot stack: whichever
// system restores last wins, and AGSPlayerCharacter::OnStartCrouch reassigns MaxWalkSpeedCrouched
// out from under it anyway.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSGE_MoveSpeedScalar.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGE_MoveSpeedScalar : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSGE_MoveSpeedScalar();
};
