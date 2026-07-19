#include "Missions/GSMissionObjective.h"

void AGSMissionObjective::BeginObjective()
{
	// Base intentionally empty - concrete objectives do their tag-search + binding here.
}

void AGSMissionObjective::ReportProgress(float Progress01)
{
	OnProgressChanged.Broadcast(FMath::Clamp(Progress01, 0.f, 1.f));
}

void AGSMissionObjective::CompleteObjective()
{
	if (bCompleted)
	{
		return;
	}
	bCompleted = true;

	ReportProgress(1.f);
	OnObjectiveCompleted.Broadcast();
}
