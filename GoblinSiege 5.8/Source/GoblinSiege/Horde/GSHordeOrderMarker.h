// The beacon the player can see. One per summoner, spawned by UGSHordeSubsystem::IssueOrder and
// retired when the order is replaced, completed or expires. Written 2026-08-12, ticket #141.
//
// WHY A NEW MARKER CLASS, given AGSRaidMarker exists and GSHordeSubsystem.cpp:233 argues against
// adding one. That argument was about a SPAWN marker, where a tag plus AGSRaidMarker::GatherByType
// genuinely did the job, and the "AGSHordeSpawnMarker will never be built" ruling still stands. None
// of it reaches this class, for four reasons that are all in AGSRaidMarker's own header:
//
//   1. AGSRaidMarker's only visuals are a UBillboardComponent and a UArrowComponent inside
//      WITH_EDITORONLY_DATA. It draws NOTHING in PIE or in a packaged game. Michael's requirement is
//      literally "markers on the map that can be seen by the player".
//   2. Its header forbids it carrying behaviour or lifetime - "a marker carries a position, a facing,
//      a type and a grouping. It carries NO spawn counts, NO patrol speed". This one carries a verb,
//      a subject, an issuer and a retirement rule.
//   3. It sets bReplicates = false on purpose. This one must replicate: "other players in the future"
//      was the point of the feature, not a nice-to-have.
//   4. GatherByType is a TActorIterator sized for "a handful of authored actors per level". Order
//      markers churn every few seconds.
//
// The class is deliberately dumb apart from that. It does not tick, it does not steer anybody, and no
// goblin ever reads it - the AI reads the subsystem. If this actor failed to spawn, every order would
// still be obeyed; the player just could not see where he had sent them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Horde/GSHordeOrderTypes.h"
#include "GSHordeOrderMarker.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class GOBLINSIEGE_API AGSHordeOrderMarker : public AActor
{
	GENERATED_BODY()

public:
	AGSHordeOrderMarker();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER ONLY. Sets the replicated payload and paints the local visuals in one call, so a listen
	 *  server never waits for its own OnRep. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Order")
	void InitialiseOrder(EGSHordeOrder InVerb, AActor* InSubject, AController* InIssuer);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Order")
	EGSHordeOrder GetVerb() const { return Verb; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Order")
	AActor* GetSubject() const { return Subject; }

	/** Who gave the order. Replicated so co-op can colour a partner's beacon differently later
	 *  without changing what goes over the wire. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Order")
	AController* GetIssuer() const { return Issuer; }

	/** SERVER ONLY. Destroys the actor; clients follow the destruction. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Horde|Order")
	void Retire();

	/**
	 * The colour for a verb - ONE table, shared by this beacon and UGSHordeOrderWheelWidget.
	 *
	 * Static and public for the same reason UGSWeaponComponent::SlotForDirection is BlueprintPure: two
	 * copies of a mapping is how a menu starts lying about what the world will do. If Attack is red on
	 * the wheel it is red on the ground, by construction rather than by discipline.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Horde|Order")
	static FLinearColor ColourForOrder(EGSHordeOrder InVerb);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Verb();

	/** Runs on the server AND on clients, both off Verb. Keeping one path rather than a server-side
	 *  setter plus a client-side OnRep body is what stops a listen server drawing something its own
	 *  client does not - the shape AGSRunicSite::ApplyPortalVisuals already uses here. */
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Horde|Order")
	TObjectPtr<USceneComponent> MarkerRoot;

	/** Assigned on BP_HordeOrderMarker, deliberately unset in C++. An invisible marker and a marker
	 *  with no mesh assigned should be the same diagnosable thing, and the art is not chosen here. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Horde|Order")
	TObjectPtr<UStaticMeshComponent> BeaconMesh;

	/** Vector parameter on the beacon material. A material without it silently keeps its authored
	 *  colour for every verb - see ApplyVisuals for why that is not detected. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order")
	FName ColourParameterName = TEXT("MarkerColour");

	/**
	 * The beacon's material, applied in BeginPlay before the MID is created.
	 *
	 * IN C++ AND NOT ON THE BLUEPRINT, and that is the whole point (#147). BeaconMesh is a NATIVE
	 * component, so a material set on BP_HordeOrderMarker's CDO lives in a component-override record
	 * that a Python write never creates - #141 set it, read it back correctly, and it silently
	 * reverted to /Engine/EngineMaterials/DefaultMaterial on the next Blueprint compile. Michael then
	 * saw grey beacons for every verb. A soft reference resolved here cannot be undone by a recompile.
	 *
	 * Soft rather than hard so a missing asset degrades to the default material instead of failing
	 * the class load, and EditDefaultsOnly so a designer can still point it somewhere else.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Horde|Order")
	TSoftObjectPtr<UMaterialInterface> BeaconMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Verb, BlueprintReadOnly, Category = "GoblinSiege|Horde|Order")
	EGSHordeOrder Verb = EGSHordeOrder::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GoblinSiege|Horde|Order")
	TObjectPtr<AActor> Subject;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GoblinSiege|Horde|Order")
	TObjectPtr<AController> Issuer;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeaconMID;
};
