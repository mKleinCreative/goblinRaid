// Minimal vertical slice of the horde follower (tech design doc §18, AGSHordeGoblin). Lightweight
// AGSCharacterBase subclass representing one AI-controlled goblin companion. This first pass only
// covers spawning + following the local player in a loose scamper (design doc §5 "Follow
// (default)"); the horn summon, Frenzy/Commanded/Courier states, the reserve-pool subsystem, and
// DetourCrowd avoidance are NOT implemented yet - see GSHordeAIController for the follow logic.
#pragma once

#include "CoreMinimal.h"
#include "Characters/GSCharacterBase.h"
#include "GSHordeGoblin.generated.h"

UCLASS()
class GOBLINSIEGE_API AGSHordeGoblin : public AGSCharacterBase
{
	GENERATED_BODY()

public:
	AGSHordeGoblin();
};
