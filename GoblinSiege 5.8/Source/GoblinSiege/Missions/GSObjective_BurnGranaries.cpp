#include "Missions/GSObjective_BurnGranaries.h"
#include "Destruction/GSDestructibleObjective.h"
#include "Kismet/GameplayStatics.h"

void AGSObjective_BurnGranaries::BeginObjective()
{
	Super::BeginObjective();

	TArray<AActor*> FoundGranaries;
	UGameplayStatics::GetAllActorsWithTag(this, GranaryActorTag, FoundGranaries);

	TotalGranaries = 0;
	BurnedGranaries = 0;

	for (AActor* Actor : FoundGranaries)
	{
		if (AGSDestructibleObjective* Granary = Cast<AGSDestructibleObjective>(Actor))
		{
			Granary->OnObjectiveDestroyed.AddDynamic(this, &AGSObjective_BurnGranaries::HandleGranaryDestroyed);
			++TotalGranaries;
		}
	}

	ReportProgress(0.f);
}

void AGSObjective_BurnGranaries::HandleGranaryDestroyed()
{
	++BurnedGranaries;

	if (TotalGranaries > 0)
	{
		ReportProgress(static_cast<float>(BurnedGranaries) / static_cast<float>(TotalGranaries));
	}

	if (BurnedGranaries >= TotalGranaries)
	{
		CompleteObjective();
	}
}
