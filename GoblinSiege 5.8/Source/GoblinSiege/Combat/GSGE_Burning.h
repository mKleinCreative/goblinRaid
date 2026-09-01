// The "on fire" status (2026-08-30, #370/#371 session): a slow periodic burn that clings to a
// pawn after it leaves the flames, unlike UGSGE_FireDamage's contact damage, which stops the
// instant you step out of a AGSFireVolume's DamageSphere.
//
// A SEPARATE effect from UGSGE_FireDamage rather than the same one with a Duration bolted on -
// UGSGE_FireDamage's own header already called this out as future work when it was written
// ("A Duration/Periodic effect would keep burning them after they left, which is a different
// (and later) design - the 'on fire' status, not standing in fire"). Keeping them apart means
// standing in a field fire always costs the fast Instant contact tick regardless of whether
// ignition succeeds, and ignition (ApplyBurningStatus in AGSFireVolume) is a separate, tunable
// decision - not every contact tick needs to set someone alight.
//
// Infinite, not HasDuration: "until you roll" is the whole design (Michael, 2026-08-30). A
// Duration would give it an independent expiry that competes with the roll as the way it ends;
// Infinite means the ONLY removal path is UGSGA_DodgeRoll::ActivateAbility calling
// RemoveActiveEffectsWithGrantedTags(State.Burning).
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSGE_Burning.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSGE_Burning : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSGE_Burning();

protected:
	// The State.Burning tag component is added here, NOT in the constructor - UGameplayEffect::
	// AddComponent<>() calls NewObject with an empty name, and the engine hard-crashes
	// ("NewObject with empty name can't be used to create default subobjects... inside of UObject
	// derived class constructor") if that runs while `this` is still under construction. Confirmed
	// against the engine's own GEComponent-upgrade code (GameplayEffect.cpp), which only ever calls
	// AddComponent/FindOrAddComponent from PostLoad-time conversion paths, never from a constructor.
	virtual void PostInitProperties() override;
};
