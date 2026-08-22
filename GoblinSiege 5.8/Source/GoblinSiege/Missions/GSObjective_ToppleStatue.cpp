#include "Missions/GSObjective_ToppleStatue.h"
#include "Destruction/GSDestructibleObjective.h"
#include "Kismet/GameplayStatics.h"

void AGSObjective_ToppleStatue::BeginObjective()
{
	Super::BeginObjective();

	TArray<AActor*> FoundStatues;
	UGameplayStatics::GetAllActorsWithTag(this, StatueActorTag, FoundStatues);

	TotalStatues = 0;
	ToppledStatues = 0;

	for (AActor* Actor : FoundStatues)
	{
		if (AGSDestructibleObjective* Statue = Cast<AGSDestructibleObjective>(Actor))
		{
			Statue->OnObjectiveDestroyed.AddDynamic(this, &AGSObjective_ToppleStatue::HandleStatueDestroyed);
			++TotalStatues;
		}
	}

	ReportProgress(0.f);
}

void AGSObjective_ToppleStatue::HandleStatueDestroyed()
{
	++ToppledStatues;

	if (TotalStatues > 0)
	{
		ReportProgress(static_cast<float>(ToppledStatues) / static_cast<float>(TotalStatues));
	}

	if (ToppledStatues >= TotalStatues)
	{
		CompleteObjective();
	}
}
