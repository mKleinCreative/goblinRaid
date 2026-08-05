// The marker set: the data contract between whoever decides WHERE things go and whoever decides
// HOW they behave. Written 2026-08-05.
//
// ---------------------------------------------------------------------------------------------
// THE BOUNDARY, AND WHY IT IS ENFORCED BY THE TYPE
//
//     Generator owns WHERE. Town owns BEHAVIOR and COUNTS.
//
// That line is from the GDD's coordination section, where it describes the contract between the
// settlement generator and the town's perception system. The spec it cites is not in this repo -
// it was shelved with the generator on 2026-07-28 - so this header is what survives of it, and the
// boundary is kept by giving this class nowhere to put a behaviour.
//
// A marker carries a position, a facing, a type and a grouping. It carries NO spawn counts, NO
// patrol speed, NO vision cone, NO archetype. Those live where they already live: counts on the
// AGSSpawnerActor subclass, stats in UGSRaceDataAsset, behaviour in the Behavior Tree. If a
// marker could say "spawn two militia here", every future layout question would become a
// negotiation about which side of the line it fell on.
//
// ---------------------------------------------------------------------------------------------
// ONE CLASS, TAG-TYPED - not eight subclasses.
//
// The same argument AGSBurnObjectiveBase::ObjectiveTypeTag already won: a new type should cost a
// tag, not a recompile. It is a stronger argument here than there, because on this machine a new
// UCLASS costs a full editor-closed build (Live Coding cannot register new UCLASS/UPROPERTY), so
// "add a marker type" must not mean "shut the editor for six minutes".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GSRaidMarker.generated.h"

class UBillboardComponent;
class UArrowComponent;

UCLASS()
class GOBLINSIEGE_API AGSRaidMarker : public AActor
{
	GENERATED_BODY()

public:
	AGSRaidMarker();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Marker")
	FGameplayTag GetMarkerType() const { return MarkerType; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Marker")
	FName GetGroupId() const { return GroupId; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Marker")
	int32 GetOrderIndex() const { return OrderIndex; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Marker")
	float GetRadius() const { return Radius; }

	/** Every marker of one type, in no particular order. TActorIterator, matching
	 *  AGSBurnObjectiveBase::FindObjectiveAtLocation - markers are a handful of actors per level
	 *  and a registry would have to survive PIE restarts to buy nothing. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Marker", meta = (WorldContext = "WorldContextObject"))
	static void GatherByType(const UObject* WorldContextObject, FGameplayTag InMarkerType,
		TArray<AGSRaidMarker*>& OutMarkers);

	/** One named group, sorted by OrderIndex - i.e. a patrol loop in walking order, or a watch's
	 *  posts in priority order. Sorting here rather than at every call site is the point. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Marker", meta = (WorldContext = "WorldContextObject"))
	static void GatherLoop(const UObject* WorldContextObject, FName InGroupId,
		TArray<AGSRaidMarker*>& OutMarkers);

protected:
	/**
	 * Which kind of marker this is - one of the Marker.* tags (see Combat/GSGameplayTags.h).
	 *
	 * EditAnywhere and per-instance: a marker's whole job is to be placed and typed in a level.
	 * Empty is a placement mistake, not a default - GatherByType will simply never return it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Marker")
	FGameplayTag MarkerType;

	/**
	 * Ties several markers into one thing: the nodes of a patrol loop, the posts of a single
	 * watch, the anchors of one civilian's routine. NAME_None means "not part of a group", which
	 * is the right answer for a lone objective anchor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Marker")
	FName GroupId = NAME_None;

	/** Position within GroupId. Meaningless outside a group; load-bearing inside one, because a
	 *  patrol loop is a sequence and not a set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Marker")
	int32 OrderIndex = 0;

	/** How much ground this marker speaks for, in unreal units. An anchor's tolerance, an arrival
	 *  zone's width. Zero means the exact point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Marker", meta = (ClampMin = "0.0"))
	float Radius = 0.f;

#if WITH_EDITORONLY_DATA
	/** Visible in the viewport and nowhere else. A marker you cannot see is a marker nobody can
	 *  review, and reviewing placement by eye is the entire point of the seed-review gate. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> MarkerBillboard;

	/** Facing matters for a guard post - "stand here" and "stand here looking that way" are
	 *  different instructions, and only one of them is reviewable without an arrow. */
	UPROPERTY()
	TObjectPtr<UArrowComponent> MarkerArrow;
#endif
};
