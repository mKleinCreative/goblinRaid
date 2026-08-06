#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeAIController.h"
#include "Combat/GSGameplayTags.h"

AGSHordeGoblin::AGSHordeGoblin()
{
	AIControllerClass = AGSHordeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// A summoned horde arrives as a crowd, which is the worst possible case for friendly fire - they
	// pack tight around whatever the horn pointed at. Same race as the player, so they cannot cut
	// each other (or him) down.
	RaceTag = GSTags::Race_Goblin;
}
