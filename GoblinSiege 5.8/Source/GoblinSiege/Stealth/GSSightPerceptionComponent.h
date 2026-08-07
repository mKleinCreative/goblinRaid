// A guard's eyes (design doc §2.4): per-target confirm timers that reset the instant line of
// sight breaks, detection range scaled by the target's crouch stance and by how much of them
// clears cover geometry, and a watchman vision cone that visibly droops on a doze timer.
// Server-authoritative - the confirm that lands here is the one soft signal the town hears.
// 2026-08-04 (Block D): first perception code in the module.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stealth/GSStealthTypes.h"
#include "GSSightPerceptionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnSightingConfirmed, AActor*, Observer, AActor*, Target);

/**
 * Drop on any human who is supposed to be able to spot a goblin. Ticks on the server only.
 * Watchmen leave bCanDoze on; roving patrols turn it off in their archetype defaults.
 */
UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSSightPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSSightPerceptionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------------------------------------------------------- queries

	/** Copy of this observer's book on Target - default-constructed if it has no book. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	FGSSightingRecord GetSightingRecord(AActor* Target) const;

	/** 0..1 progress toward the 1.5s hold for a specific target. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	float GetConfirmProgress01(AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	bool HasConfirmedSighting(AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	bool HasAnyConfirmedSighting() const;

	/** Where this guard is actually looking, droop included. Aim the cone art at this. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	FVector GetVisionDirection() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	FVector GetEyeLocation() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	float GetDozeAlpha() const { return DozeTuning.GetDozeAlpha(DozeSeconds); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	float GetCurrentDroopDegrees() const { return DozeTuning.MaxDroopDegrees * GetDozeAlpha(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	float GetEffectiveHalfAngleDegrees() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	float GetDozeRangeMultiplier() const;

	// ---------------------------------------------------------------- commands

	/** Snap the guard awake - call from noise, damage, corpse sightings, coin tosses. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void Rouse();

	/** Wipe every book. Used on unaware-state resets and takedown cleanup. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void ForgetAll();

	/** Fired on the server the frame a hold completes, before the subsystem signal goes out. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stealth")
	FGSOnSightingConfirmed OnSightingConfirmed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Stealth")
	FGSDetectionTuning Tuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Stealth")
	FGSDozeTuning DozeTuning;

	/** Draws the (drooping) cone and the confirm state. Cheap enough to leave on in PIE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Stealth")
	bool bDrawDebugVisionCone = false;

protected:
	/** Cone + range + cover test. OutExposure01 is the fraction of body samples clearing cover. */
	bool EvaluateVisibility(AActor* Target, float& OutExposure01) const;

	void ConfirmSighting(AActor* Target, FGSSightingRecord& Record);

	void UpdateDoze(float DeltaTime, bool bSawSomeone);

	void DrawDebugVision(float DeltaTime) const;

private:
	/** Weak keys: a target that dies or streams out just drops off the books. Not reflected, so
	 *  no UPROPERTY - weak pointers keep nothing alive. */
	TMap<TWeakObjectPtr<AActor>, FGSSightingRecord> Sightings;

	float DozeSeconds = 0.f;
};
