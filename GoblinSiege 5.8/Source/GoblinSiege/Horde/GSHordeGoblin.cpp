#include "Horde/GSHordeGoblin.h"
#include "Horde/GSHordeAIController.h"

AGSHordeGoblin::AGSHordeGoblin()
{
	AIControllerClass = AGSHordeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
