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
class UGSInteractableComponent;

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

	/**
	 * Make an actor that ALREADY EXISTS in a level carryable loot, without replacing it.
	 *
	 * WHY IN PLACE RATHER THAN SWAPPED FOR A BLUEPRINT: Tutorial Island is dressed with 46
	 * `A_Pig_*` actors that are plain SkeletalMeshActors playing A_Pig_Eat / SitLoop / SleepLoop as
	 * single-node animations. They look exactly like livestock and are not - no interactable, no
	 * loot value - so the player walks up to a pig and nothing happens, which reads as the pickup
	 * being broken rather than as that pig being scenery. Michael hit this in play.
	 *
	 * Swapping each one for BP_Livestock_Pig would work and would throw away the dressing: the
	 * per-instance animation, the pose variety, the placement. This adds the component and touches
	 * NOTHING else, so the pig goes on eating and can now be picked up.
	 *
	 * Same AddInstanceComponent reason as MakeActorFlammable and MakeActorBreakable - see above.
	 * A component added any other way from Python vanishes on the next level load.
	 *
	 * LootValue follows GDD 10: pig 40, sheep 25, chicken 10. Passing 0 is legal and means "carryable
	 * but worth nothing" - the component's own rule is that zero value is NOT loot.
	 *
	 * Idempotent: returns the existing component rather than stacking a second one.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static UGSInteractableComponent* MakeActorCarryable(AActor* Actor, int32 LootValue,
		FText PromptText, float ChannelSeconds = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid|Scripting")
	static int32 CountCarryable(const TArray<AActor*>& Actors);

	/**
	 * Damage every breakable prop inside a swing arc. Returns how many were hit.
	 *
	 * WHY THIS EXISTS: until now nothing the player swings could break anything. The blocker was
	 * never the hostility check - AGSCharacterBase::IsHostileTo already returns true for a prop, and
	 * says so in a comment. It was that UGSGA_SwordLight::DoSweep queries ECC_Pawn only, so a
	 * StaticMeshActor never entered the result set at all, and that the loop below it requires both
	 * actors to have an AbilitySystemComponent. A crate has neither an ASC nor a race tag.
	 *
	 * So this is a SECOND, separate overlap rather than more object types on the existing one. Props
	 * must never fall into the GAS path below - guard-break, recoil, frenzy and the hit set are all
	 * about characters, and a barrel entering any of them is a bug waiting to happen.
	 *
	 * Deliberately NOT a UFUNCTION: a TSet by-reference out-param is awkward through UHT, and every
	 * caller (the sword ability, the horde's smash task) is C++.
	 *
	 * @param AlreadyHit  the caller's per-swing hit set, so one swing smashes each prop exactly once.
	 */
	static int32 SmashBreakablesInArc(const UObject* WorldContextObject, AActor* Instigator,
		const FVector& Origin, float Radius, const FVector& Forward, float ArcDegrees,
		int32 Damage, TSet<TObjectPtr<AActor>>& AlreadyHit);
};
