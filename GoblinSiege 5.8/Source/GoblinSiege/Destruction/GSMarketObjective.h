// The market: a CLUSTER burn (ruled 2026-07-28). Fire takes one stall, awnings and goods carry it
// to its neighbours, and the objective completes when most of the cluster is alight-and-gone.
// Written 2026-07-28 for Block C.
//
// This objective deliberately implements almost nothing itself. The stalls are ordinary actors
// carrying UGSFlammableComponent with bCanSpread = true, so the fire-spread system already built
// does the propagation; the market just adopts the cluster, watches it, and counts. That is the
// point of having spread be a component behaviour rather than per-objective code.
//
// DESIGN NOTE - the lesson this objective teaches: loot in a burning stall burns with it. Robbing
// the market before torching it is strictly better play, and the market is the one objective where
// the greed/safety tension of §9 is expressed in a single decision. When the loot system exists,
// bind OnStallBurnedDown to destroy that stall's unlooted contents.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSMarketObjective.generated.h"

class UGSFlammableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnStallBurnedDown, AActor*, Stall);

UCLASS()
class GOBLINSIEGE_API AGSMarketObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSMarketObjective();

	/** A torch landing in the market lights the nearest stall; spread does the rest. */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	/** Within AutoAdoptRadius of the market centre. */
	virtual bool ContainsWorldLocation(const FVector& WorldLocation) const override;

	/**
	 * The brigade's verb against the market. Extinguishes burning stalls within Radius.
	 *
	 * Unlike the field there is no firebreak concept here - a stall is a discrete structure, so
	 * UGSFlammableComponent::Extinguish() is the whole mechanic and stalls stay re-lightable by
	 * their normal rules. UBTTask_Firefight already finds these stalls unaided (it targets the
	 * nearest burning flammable), so this exists for the objective-level and debug paths.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Market")
	int32 DouseAtLocation(const FVector& WorldLocation, float Radius);

	/** How many stalls are alight right now. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Market")
	int32 GetBurningStallCount() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Market")
	int32 GetStallCount() const { return Stalls.Num(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Market")
	int32 GetBurntStallCount() const { return BurntStallCount; }

	/** Fires as each stall goes. The loot layer binds this to burn unlooted goods. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Market")
	FGSOnStallBurnedDown OnStallBurnedDown;

protected:
	virtual void BeginPlay() override;

	virtual void DrawDebugState() const override;

	/** Finds the stall actors that make up this market. */
	void AdoptCluster();

	UFUNCTION()
	void HandleStallBurnedDown();

	/**
	 * Explicit stall list. Leave empty to auto-adopt every actor with a UGSFlammableComponent
	 * inside AutoAdoptRadius at BeginPlay - which is how a kitbashed market of 28 stall
	 * structures gets wired without hand-linking every one.
	 */
	UPROPERTY(EditInstanceOnly, Category = "GoblinSiege|Market")
	TArray<TObjectPtr<AActor>> Stalls;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Market")
	bool bAutoAdoptCluster = true;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Market", meta = (EditCondition = "bAutoAdoptCluster"))
	float AutoAdoptRadius = 3000.f;

	/**
	 * Only adopt actors whose name contains one of these. Stops the market swallowing every
	 * fence and barrel in the square - it should be the stalls that constitute the objective.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Market", meta = (EditCondition = "bAutoAdoptCluster"))
	TArray<FString> AutoAdoptNameFilters = { TEXT("Stall"), TEXT("Market") };

	int32 BurntStallCount = 0;

	/** Stall count captured once at BeginPlay. Used as the completion denominator so a stall
	 *  actor destroying itself after burning down can't shrink the target from under us. */
	int32 InitialStallCount = 0;

	/** Stalls we've already fired OnStallBurnedDown for. The recount below is idempotent, but the
	 *  event must not be - burning a stall's loot twice is a bug waiting for the loot system. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> ReportedStalls;
};
