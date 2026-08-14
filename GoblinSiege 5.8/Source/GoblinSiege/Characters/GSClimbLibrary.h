// Climb ledge detection, moved out of Blueprint.
//
// The Blueprint version asked "is there a walkable surface exactly N units inboard of the wall?"
// with N a hand-tuned constant. Measured across 56 roof lips on 14 houses, no single N clears more
// than 88% - the roofs are ribbed at ~40uu, so the answer alternates between the deck and a rib as
// N moves. Searching the range clears 96%. That search is a loop, which is why it lives here.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSClimbLibrary.generated.h"

/** What the ledge search decided, and why - so a failure is readable without guessing. */
USTRUCT(BlueprintType)
struct FGSClimbLedgeResult
{
	GENERATED_BODY()

	/** A ledge worth mantling onto was found. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	bool bFound = false;

	/** World location of the deck surface to mantle onto. Only meaningful when bFound. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	FVector Deck = FVector::ZeroVector;

	/** How far inboard of the wall the accepted deck was found. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	float Inset = 0.f;

	/** Vertical rise from the climber to the deck. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	float Rise = 0.f;

	/** Surface normal Z of the accepted deck (1.0 = flat). */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	float NormalZ = 0.f;

	/** Insets that hit something but were rejected as too steep. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	int32 RejectedSteep = 0;

	/** Insets that found a walkable surface with a roof above it - an interior floor, not a ledge. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	int32 RejectedInterior = 0;

	/** Insets whose probe hit nothing at all. */
	UPROPERTY(BlueprintReadOnly, Category = "Climb")
	int32 RejectedEmpty = 0;
};

UCLASS()
class GOBLINSIEGE_API UGSClimbLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Search inboard of the wall for a ledge the climber can mantle onto.
	 *
	 * Sweeps a sphere downward at a series of insets and accepts the first surface that is both
	 * walkable AND has open sky above it. The sky test is what separates a roof from an interior
	 * floor: since SM_MERGED_House_Medium_11 went to UseComplexAsSimple there is no wall left to
	 * stop a downward probe reaching a floor inside the building, and those read as perfectly
	 * walkable (normal.Z 1.0). Without the sky test the mantle can put the player inside the house.
	 *
	 * @param Climber      The climbing character. Its CharacterMovement supplies the walkable limit.
	 * @param WallNormal   Outward normal of the wall being climbed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GS|Climb")
	static FGSClimbLedgeResult FindClimbLedge(ACharacter* Climber, FVector WallNormal);

	/**
	 * Can the climber's capsule still rise? Stateless replacement for the ClimbBlockedSeconds
	 * accumulator, which needed several consecutive frames to agree and could be reset by any
	 * single frame of movement - including the movement the lean-out itself produced.
	 */
	UFUNCTION(BlueprintCallable, Category = "GS|Climb")
	static bool IsClimbBlockedUpward(ACharacter* Climber, float RiseDistance = 45.f);
};
