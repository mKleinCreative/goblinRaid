// A building that can be set alight - through a window, or across its roof.
// Written 2026-08-05 (Michael's flow: "a torch gets thrown into a window ... or onto the ceiling").
//
// ---------------------------------------------------------------------------------------------
// WHY THIS CLASS EXISTS AT ALL: there was no building.
//
// A house on L_Tutorial_Island is not a mesh and not an actor. It is 20-40 loose StaticMeshActors
// kitbashed together - floors of 12 triangles, walls, corners, beams, roof segments, windows -
// out of 150 distinct piece types and 1,413 instances across the map. Nothing owns a house, so
// "the building catches fire" had nothing to happen TO. This actor is the missing noun: it adopts
// the pieces standing inside its footprint and becomes the thing that burns, scores and completes.
//
// It is deliberately the same shape as AGSMarketObjective, which already solves "adopt a cluster of
// flammable props and complete at a fraction of them" - and it inherits that class's hard-won rule:
//
//     A CLUSTER OBJECTIVE'S ADOPT RADIUS MUST NOT EXCEED WHAT ITS SPREAD DISTANCE CAN TRAVERSE.
//
// The market spent a session unwinnable because it adopted 64 scattered stalls when fire could only
// ever reach ~30 of them. Adopting more pieces makes a building HARDER to burn, not richer, and
// nothing anywhere reports the shortfall. Hence AdoptRadius defaults tight and the placement script
// derives it from real geometry.
//
// ---------------------------------------------------------------------------------------------
// THE IGNITION RULE (the whole point of the feature)
//
// A stone-and-plaster house does not catch because you held a torch to its outside wall. Like the
// windmill - which refuses exterior fire and is reachable only through its windows - a building has
// exactly two ways in:
//
//   1. A WINDOW. Break it and the torch goes through. See UGSBreakableComponent.
//   2. THE ROOF. Thatch and beams, lit from above.
//
// Everything else is refused, and ContainsWorldLocation returns false for the walls so the torch's
// own objective lookup will not light a house by splashing its facade.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSBuildingObjective.generated.h"

class UGSFlammableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBuildingIgnited);

/** How a building was lit - for barks, score, and (later) which door civilians flee out of. */
UENUM(BlueprintType)
enum class EGSBuildingIgnitionSource : uint8
{
	None,
	Window,
	Roof,
	Spread,
	Debug
};

UCLASS()
class GOBLINSIEGE_API AGSBuildingObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSBuildingObjective();

	/**
	 * Light the inside. THE entry point - a broken window calls this, the roof calls this, the
	 * debug command calls this. Idempotent: a building already alight ignores further torches.
	 *
	 * Server-only. Named to match AGSMillObjective::IgniteInterior, which is the same idea on the
	 * one building that already had it.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Building")
	void IgniteInterior(EGSBuildingIgnitionSource Source = EGSBuildingIgnitionSource::Debug);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	bool IsAlight() const { return bAlight; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	int32 GetPieceCount() const { return Pieces.Num(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	int32 GetBurntPieceCount() const { return BurntPieceCount; }

	/** How this building was first lit. Kept because it is the seam the fleeing-civilian work will
	 *  want (Michael, 2026-08-05: civilians flee burning buildings out the door - EARMARKED, not
	 *  built): a house lit through the roof should read differently from one lit at the window a
	 *  civilian is standing beside. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	EGSBuildingIgnitionSource GetIgnitionSource() const { return IgnitionSource; }

	/** Fires once, server-side, the moment the building goes up. The hook the civilian-flee work
	 *  binds to later. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Building")
	FGSOnBuildingIgnited OnBuildingIgnited;

	/** The building owning this piece, or null. Used by the torch and by breakable windows, and by
	 *  the same TActorIterator reasoning as AGSBurnObjectiveBase::FindObjectiveAtLocation:
	 *  buildings are tens of actors per level, not thousands. */
	static AGSBuildingObjective* FindBuildingOwning(const UObject* WorldContextObject, AActor* Piece);

	/**
	 * Is this piece part of the roof?
	 *
	 * Public and separate from the window path because the torch has to tell them apart: a window is
	 * broken and passed through, a roof is simply lit from above. Both are "entry", only one is a
	 * roof, and collapsing the two would make a torch bounced off a windowpane light the house
	 * without ever breaking it.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	bool IsRoofPiece(const AActor* Piece) const;

	/** Walls refuse fire; only windows and roof let it in. */
	virtual bool ContainsWorldLocation(const FVector& WorldLocation) const override;

	/** Deliberately does nothing. A torch that splashes a wall must not light the house - that is
	 *  the whole ignition rule, and the base class's default is already "an objective owns no
	 *  ground", so this override exists purely to say the silence is intentional. */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** Sweep the footprint and take ownership of the kit pieces inside it. */
	void AdoptPieces();

	/** Give every adopted piece a flammable component if it has none, so a kitbashed house can burn
	 *  at all. Mirrors UGSRaidLibrary::MakeActorFlammable, which exists because the market's stalls
	 *  had the same problem: plain StaticMeshActors cannot catch. */
	void EnsurePiecesFlammable();

	UFUNCTION()
	void HandlePieceBurnedDown();

	void RecomputeCompletion();

	/** True if this piece reads as a way in (roof or window) rather than a wall. Name-based, like
	 *  the market's stall filter - the kit has no metadata to ask, and a mesh naming convention is
	 *  the only signal the art actually carries. */
	bool IsEntryPiece(const AActor* Piece) const;

	UFUNCTION()
	void OnRep_Alight();

	// ------------------------------------------------------------------ tuning

	/**
	 * Footprint. Pieces inside this are the building.
	 *
	 * Kept tight on purpose - see the class comment. The placement script sizes it from the actual
	 * cluster rather than trusting this default, which exists only so a hand-placed building is
	 * sane out of the box.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning", meta = (ClampMin = "100.0"))
	float AdoptRadius = 1200.f;

	/** Substrings that mark a piece as a way IN rather than a wall (windows and roof). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> EntryNameFilters;

	/** Substrings that mark a piece as ROOF specifically - the half of "entry" a torch can light
	 *  from above without breaking anything. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> RoofNameFilters;

	/** Substrings a piece's mesh must match to be adopted at all. Empty adopts anything with a
	 *  static mesh inside the radius, which drags in barrels and market tables. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> PieceNameFilters;

	/** Alarm the moment the house goes up. A burning building is unmissable; louder than a field. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	float AlarmOnIgnite = 15.f;

	// ------------------------------------------------------------------ state

	/** Adopted kit pieces. Weak: a piece destroyed by other means simply drops out. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Pieces;

	UPROPERTY()
	TArray<TWeakObjectPtr<UGSFlammableComponent>> PieceFlammables;

	/** Set at adoption, so completion has a stable denominator even as pieces are destroyed. */
	int32 InitialPieceCount = 0;

	int32 BurntPieceCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Alight)
	bool bAlight = false;

	EGSBuildingIgnitionSource IgnitionSource = EGSBuildingIgnitionSource::None;
};
