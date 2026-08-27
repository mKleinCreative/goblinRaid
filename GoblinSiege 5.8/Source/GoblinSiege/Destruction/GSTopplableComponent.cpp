#include "Destruction/GSTopplableComponent.h"

#include "Destruction/GSCrumbleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Raid/GSScoreSubsystem.h"
#include "Engine/World.h"
#include "World/GSCorruptionSubsystem.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTopple, Log, All);

UGSTopplableComponent::UGSTopplableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSTopplableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTopplableComponent, bToppled);
}

FText UGSTopplableComponent::GetObjectiveDisplayName() const
{
	if (!ObjectiveDisplayName.IsEmpty())
	{
		return ObjectiveDisplayName;
	}

	// Derived from the tag leaf, the same way the HUD pluralises burn types: a fifth monument type
	// should cost a tag and nothing else. Objective.Topple.Statue -> "Statue".
	if (ObjectiveTypeTag.IsValid())
	{
		FString Leaf = ObjectiveTypeTag.ToString();
		int32 Dot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), Dot))
		{
			Leaf = Leaf.RightChop(Dot + 1);
		}
		return FText::FromString(Leaf);
	}

	return NSLOCTEXT("GoblinSiege", "MonumentObjectiveDefault", "Monument");
}

bool UGSTopplableComponent::WardReaches(const FVector& Spot) const
{
	if (bWardsEntireLevel)
	{
		return true;
	}

	const AActor* Monument = GetOwner();
	if (!Monument || WardRadius <= 0.f)
	{
		return false;
	}

	// 2D, deliberately. A monument in a graveyard below the hamlet it guards would otherwise let the
	// player climb the slope and plant a gate 900 uu away on the map but "outside" the ward purely on
	// height.
	return FVector::DistSquared2D(Monument->GetActorLocation(), Spot) <= FMath::Square(WardRadius);
}

bool UGSTopplableComponent::Topple(AActor* Toppler, const FVector& PullDirection, const FVector& AnchorPoint)
{
	// A monument comes down once. Not defensive - the haul ticks every frame and would otherwise
	// re-push a statue that is already mid-fall, which reads as it being swatted.
	if (bToppled)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	// Push along the pull, with a little lift so it goes OVER rather than skidding. Applied at the
	// rope's anchor - high up on a statue - so the force has a moment arm and the thing rotates about
	// its base. Applied at the centre of mass it would simply slide.
	FVector Direction = PullDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector();
	}
	Direction = (Direction + FVector::UpVector * ToppleLift).GetSafeNormal();

	// ---- hand the release over -----------------------------------------------------------------
	// The swap, the promotion to dynamic, the anchors and the one-frame defer all used to live here,
	// and three of the four were found the hard way on 2026-08-19. They now live in
	// UGSCrumbleComponent, because a building burning down needs the identical sequence and a second
	// copy of it is a second copy to get wrong - the copy in AGSDestructibleObjective was missing two
	// of the four steps and had never worked.
	//
	// What stays here is what makes this a TOPPLE rather than a collapse: the direction comes from the
	// rope, and the shove lands at the anchor rather than the centre of mass.
	//
	// FindOrAdd rather than a required component, so a monument placed as a plain StaticMeshActor
	// works without anybody hand-wiring it.
	UGSCrumbleComponent* Crumble = UGSCrumbleComponent::FindOrAdd(Owner);
	if (Crumble)
	{
		// Carried across rather than dropped. The two default to the same name, so this changes
		// nothing on the statue - but a monument that authored a different one before the crumble
		// component existed must not silently stop swapping its mesh, and a swap that does not
		// happen looks exactly like a topple that did not work.
		Crumble->IntactMeshComponentName = IntactMeshComponentName;
	}

	if (!Crumble || !Crumble->Crumble(Direction * ToppleImpulse, AnchorPoint))
	{
		// Refused - almost always no geometry collection, which Crumble has already logged by name.
		// The monument stays standing and bToppled stays false, so the haul can be tried again rather
		// than the rope going dead on a statue that never moved.
		return false;
	}

	bToppled = true;

	UE_LOG(LogGSTopple, Log,
		TEXT("[GoblinSiege] '%s' toppled by '%s' - impulse %.0f along (%.2f, %.2f, %.2f) at the rope anchor."),
		*Owner->GetName(),
		Toppler ? *Toppler->GetName() : TEXT("<none>"),
		ToppleImpulse, Direction.X, Direction.Y, Direction.Z);

	// World corruption (ruling 40). Deliberately OUTSIDE the score block below: a toppled idol
	// still turns the land even on a map with no score subsystem, and nesting it there would make
	// corruption silently depend on scoring being present.
	if (UGSCorruptionSubsystem* Corruption = UGSCorruptionSubsystem::Get(this))
	{
		Corruption->ReportStructureDestroyed(Owner);
	}

	// ---- score it ------------------------------------------------------------------------------
	// Awarded HERE rather than from a listener on OnToppled, for the reason the idempotence guard at
	// the top of this function exists: the topple is already first-wins, so scoring inside it cannot
	// double-count. A subscriber could be bound twice and silently award twice, and a wrong score is
	// the kind of bug nobody notices until an end-of-raid screen looks odd.
	if (UWorld* World = GetWorld())
	{
		if (UGSScoreSubsystem* Score = World->GetSubsystem<UGSScoreSubsystem>())
		{
			// First of its type scores full, later ones the duplicate rate - the same demotion the
			// burn objectives use, so a hamlet with three idols does not out-score the whole raid.
			const bool bFirstOfType = !DeedTypeTag.IsValid() || Score->GetDeedsForType(DeedTypeTag) <= 0;
			const int32 Points = bFirstOfType ? ToppleDeeds : DuplicateToppleDeeds;

			Score->AddDeeds(DeedTypeTag, Points);

			UE_LOG(LogGSTopple, Log,
				TEXT("[GoblinSiege] '%s' scored +%d deeds (%s %s) -> %d total."),
				*Owner->GetName(), Points,
				bFirstOfType ? TEXT("first") : TEXT("duplicate"),
				DeedTypeTag.IsValid() ? *DeedTypeTag.ToString() : TEXT("untagged"),
				Score->GetTotal());
		}
	}

	// Deliberately NOT calling Break(). The statue is not broken yet - it is falling. It shatters when
	// it lands, through bEnableDamageFromCollision on the collection. See the header.
	OnToppled.Broadcast(Toppler);
	return true;
}

void UGSTopplableComponent::ResetTopple()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		bToppled = false;
	}

	// The mesh swap, the collection and the physics state all belong to the crumble component now, so
	// standing a monument back up is its inverse and not a second hand-written one. Not authority-
	// gated on its side, deliberately - see UGSCrumbleComponent::ResetCrumble.
	if (UGSCrumbleComponent* Crumble = Owner->FindComponentByClass<UGSCrumbleComponent>())
	{
		Crumble->ResetCrumble();
	}

	UE_LOG(LogGSTopple, Log, TEXT("[GoblinSiege] '%s' stood back up."), *Owner->GetName());
}

// Console: stand every monument in the play world back up, so the haul can be retried without
// restarting PIE. Cheat-only - this exists to make the feature testable, not playable.
static FAutoConsoleCommandWithWorld GSToppleResetAllCmd(
	TEXT("GS.Topple.ResetAll"),
	TEXT("Stand every toppled monument back up (testing)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		int32 Count = 0;
		for (TObjectIterator<UGSTopplableComponent> It; It; ++It)
		{
			UGSTopplableComponent* Topplable = *It;
			// World-filtered: without this an editor-world CDO or another PIE instance gets reset too.
			if (IsValid(Topplable) && Topplable->GetWorld() == World)
			{
				Topplable->ResetTopple();
				++Count;
			}
		}
		UE_LOG(LogGSTopple, Log, TEXT("[GoblinSiege] GS.Topple.ResetAll stood up %d monument(s)."), Count);
	}));
