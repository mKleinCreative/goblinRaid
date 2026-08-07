// Scripted-placement helpers: the things a Python script (and, later, the settlement generator)
// needs to do to a level that editor Python cannot express on its own.
// Written 2026-08-05.
//
// WHY THIS EXISTS - two hard walls hit while wiring Tutorial Island from Python:
//
//   1. FGameplayTag cannot be constructed in editor Python in this build. There is no
//      request_gameplay_tag exposed on GameplayTagLibrary, FGameplayTag::TagName is read-only,
//      the struct takes no constructor arguments, and FGameplayTagContainer::GameplayTags is
//      read-only too. Every burn carrier's ObjectiveTypeTag is per-instance and set by NOBODY in
//      C++ by design - so without a bridge, a script can place an objective but can never make it
//      count toward the win condition.
//
//   2. A component added from Python does not reliably survive a level save. The API that makes an
//      instance component persist is AActor::AddInstanceComponent, which is not exposed.
//      This blocks the market entirely: AGSMarketObjective::AdoptCluster only adopts actors that
//      carry a UGSFlammableComponent, and Tutorial Island's ~130 stall and market-table actors are
//      plain StaticMeshActors. The market silently adopts zero of them and logs an error the player
//      never sees.
//
// Both are equally needed by the eventual generator, which has to tag carriers and dress props
// programmatically - so this is on that critical path, not a one-off scaffold for a single map.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GSRaidLibrary.generated.h"

class AGSBurnObjectiveBase;
class AGSRaidMarker;
class UGSFlammableComponent;
class UGSBreakableComponent;

UCLASS()
class GOBLINSIEGE_API UGSRaidLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * FGameplayTag from a name, or an empty tag if it is not registered.
	 *
	 * bErrorIfNotFound defaults true because the failure this guards against is silent and
	 * expensive: an unregistered tag yields an EMPTY tag, an untagged carrier is invisible to the
	 * demotion pass by design, and the level then looks fine while being unwinnable. A typo should
	 * be loud here rather than discovered in a playtest.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static FGameplayTag MakeTagByName(FName TagName, bool bErrorIfNotFound = true);

	/**
	 * Set a placed burn carrier's type tag and HUD name in one call.
	 *
	 * A designer does this in the Details panel; this is the same edit for a script. Returns false
	 * and logs if the tag name does not resolve, rather than quietly writing an empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static bool SetObjectiveIdentity(AGSBurnObjectiveBase* Objective, FName TypeTagName, FText DisplayName);

	/** Marker placement in one call - see AGSRaidMarker for what a marker may and may not carry. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static bool ConfigureMarker(AGSRaidMarker* Marker, FName MarkerTypeName, FName GroupId,
		int32 OrderIndex, float Radius);

	/**
	 * Give an actor a UGSFlammableComponent so it can burn - and so AGSMarketObjective will adopt
	 * it as a stall.
	 *
	 * Uses AddInstanceComponent + RegisterComponent, which is what makes the component persist in
	 * the saved level rather than evaporating on reload. Idempotent: an actor that already has one
	 * gets its existing component back, so re-running a dressing script cannot stack components.
	 *
	 * Returns null for a null actor. Marks the package dirty on success so the level save picks it up.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static UGSFlammableComponent* MakeActorFlammable(AActor* Actor);

	/** How many of these actors already carry a flammable component - the cheap way for a script to
	 *  verify a dressing pass did what it claimed. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static int32 CountFlammable(const TArray<AActor*>& Actors);

	/**
	 * Nearest spot near Origin where a pawn-sized capsule can actually stand.
	 *
	 * Ground-traces down, then tests whether the capsule FITS - the second half being the check that
	 * matters, because "a capsule fits here" and "this is outdoors" are different questions and the
	 * first spawn fix for the runic site picked the inside of a house by only asking the first
	 * (#009). Widens through rings of bearings if the origin itself is no good.
	 *
	 * Extracted from AGSRunicSite's private spawn search so a drowning can reuse it, seeded at the
	 * water instead of at the site. Deliberately NOT navmesh projection: GEN_NavBounds_Village is
	 * only 4000x4000 uu, so projection fails across most of the map (AGENT_STATE).
	 *
	 * @param IgnoreActor   Excluded from both traces - pass the pawn doing the asking.
	 * @return false if nothing standable was found in any ring; OutSpot is untouched.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting",
		meta = (WorldContext = "WorldContextObject"))
	static bool FindStandableSpotNear(const UObject* WorldContextObject, FVector Origin,
		FVector& OutSpot, const AActor* IgnoreActor = nullptr);

	/**
	 * Give an actor a UGSBreakableComponent - so a window can be smashed and become a way into a
	 * building.
	 *
	 * Exists for the same reason MakeActorFlammable does, and was added after making the same
	 * mistake a second time: the placement script tried `add_component_by_class` from Python and
	 * that method does not exist on a StaticMeshActor, so 113 windows silently got zero components.
	 * Even where a Python path exists it does not route through AddInstanceComponent, and a
	 * component that skips that vanishes on the next level load - the level looks dressed until you
	 * reopen it.
	 *
	 * Idempotent: returns the existing component rather than stacking a second one.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static UGSBreakableComponent* MakeActorBreakable(AActor* Actor, bool bOpensBuilding = true);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static int32 CountBreakable(const TArray<AActor*>& Actors);
};
