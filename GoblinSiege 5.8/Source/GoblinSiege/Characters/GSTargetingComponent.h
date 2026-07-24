// Camera-forward soft-lock targeting (tech doc §16): scores nearby defenders by angle-to-camera
// and distance inside a narrow cone, exposing a CurrentSoftTarget for aim-assist bend on ranged
// attacks and for the Horde Agent's future point-command target resolution. No hard lock - melee
// keeps generous forward hit arcs instead (design doc §7 "no hard lock-on").
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTargetingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnSoftTargetChanged, AActor*, NewTarget);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTargetingComponent();

	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Targeting")
	AActor* GetCurrentSoftTarget() const { return CurrentSoftTarget.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Targeting")
	FGSOnSoftTargetChanged OnSoftTargetChanged;

protected:
	/** Re-scores nearby candidates and updates CurrentSoftTarget. Runs on a throttled tick interval
	 *  (ScanIntervalSeconds), not every frame - this is aim-assist flavor for a soft lock, not a
	 *  competitive hitscan system, so it doesn't need per-frame precision. */
	void RefreshSoftTarget();

	/** Half-angle of the cone (from camera forward) candidates must fall inside to be considered
	 *  (tech doc §16: "~6° cone by angle + distance"). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Targeting|Tuning")
	float ConeHalfAngleDegrees = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Targeting|Tuning")
	float MaxTargetDistance = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Targeting|Tuning")
	float ScanIntervalSeconds = 0.1f;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentSoftTarget;
};
