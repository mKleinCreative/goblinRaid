// The windmill: the objective that has to be SOLVED rather than merely torched (design doc §6.3).
// Stone base, no purchase, and exterior fire alone won't take it - the goblin has to put a torch
// through an upper window into the flour dust inside, at which point the mill is on a fuse.
// Written 2026-07-28 for Block C.
//
// Sequence: Intact -> (torch through window) -> Smouldering [dust builds] -> Detonated -> state swap.
//
// The refusal is the teaching moment. Throwing fire at the outside does nothing mechanically but
// DOES fire OnExteriorIgnitionRefused, so the bark system can have the goblin mutter about the
// stone and the player learns to look up. A silent no-op would just read as a bug.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSMillObjective.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;
class UGSBurnFXComponent;
class URotatingMovementComponent;

UENUM(BlueprintType)
enum class EGSMillStage : uint8
{
	Intact,
	Smouldering,
	Detonated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnMillSimple);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnMillStageChanged, EGSMillStage, NewStage);

UCLASS()
class GOBLINSIEGE_API AGSMillObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSMillObjective();

	/**
	 * Exterior fire. Does NOT light the mill - stone doesn't take, and the sails are too high to
	 * reach from the ground. Broadcasts the refusal so the goblin can say so out loud.
	 */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	/**
	 * The real verb: a lit torch has passed through a window and into the flour dust. Starts the
	 * fuse. Idempotent - a second torch through a second window changes nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Mill")
	void IgniteInterior();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Mill")
	EGSMillStage GetStage() const { return Stage; }

	/** Seconds until detonation, or 0 if not lit / already gone. Drives the audio build. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Mill")
	float GetSecondsToDetonation() const;

	/** Bark hook: "Won't take. Stone don't burn." */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnExteriorIgnitionRefused;

	/** The fuse is lit - dust glow, interior light, rising rumble. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnInteriorIgnited;

	/** The moment. Camera shake, the big VFX, and every alarm in the hamlet. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillSimple OnMillDetonated;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Mill")
	FGSOnMillStageChanged OnMillStageChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleCompleted() override;
	virtual void DrawDebugState() const override;

	void BindWindowVolumes();
	void Detonate();
	void SetStage(EGSMillStage NewStage);
	void TickBuildup();

	UFUNCTION()
	void OnWindowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Stage();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill")
	TObjectPtr<UStaticMeshComponent> MillMesh;

	/**
	 * The sails, attached to the mill body. Assign a mesh in the Blueprint (the map's dressing
	 * mills use SM_Windmill_Sail); left unset the mill simply has no sails and none of the spin
	 * machinery does anything.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<UStaticMeshComponent> SailMesh;

	/**
	 * Q-34 ruling (2026-08-01): the sails keep turning while the interior burns - Michael:
	 * "having the windmill spinning while burning would be a great sight" - and stop dead at
	 * detonation, which reads as the machine dying even before any ruin mesh lands.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<URotatingMovementComponent> SailSpin;

	/**
	 * Sail turn rate, degrees per second. Which axis is right depends on how the sail mesh is
	 * authored; SM_Windmill_Sail turns about its local X, hence the default roll. Set all zeros
	 * in a Blueprint to park the sails entirely.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Visual")
	FRotator SailSpinRate = FRotator(0.f, 0.f, 25.f);

	/**
	 * Char driver (2026-07-31, Q-34). The mill scorches as its fuse burns down, delivering the
	 * burn-down spec's 0.33 / 0.66 / 1.0 char steps, WITH NO CHANGE TO ITS IGNITION RULES.
	 *
	 * The obvious way to get char on this actor would be to give it a UGSFlammableComponent, since
	 * that is what UGSBurnFXComponent normally reads. That is exactly the wrong move and it is
	 * worth saying so here, because it will look like an oversight to the next person: the mill
	 * deliberately has no flammable component. It runs its own Intact -> Smouldering -> Detonated
	 * dust-fuse state machine precisely so that exterior fire CANNOT take it (design doc §6.3 -
	 * "stone base, sails out of reach"), and a flammable component would hand it a second,
	 * contradictory ignition path: AGSFireVolume::SpreadTick and AGSMillObjective::Detonate both
	 * call Ignite() on every flammable in reach, so a hedge fire next door - or another mill - would
	 * light this one through a rule the design specifically refuses. It would also re-arm the town
	 * alarm twice and give the mill a second, unrelated completion clock in BurnDurationSeconds.
	 *
	 * So the FX component is driven DIRECTLY instead, from TickBuildup and OnRep_Stage, and the
	 * mill keeps exactly one definition of what "burning" means for it. UGSBurnFXComponent supports
	 * this: with no flammable present its BeginPlay logs once and skips the auto-drive, leaving
	 * SetBurnAmount as a pure material driver. Verified 2026-07-31 - it does not warn per frame and
	 * does not crash, so no bRequireFlammable opt-out was needed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Mill")
	TObjectPtr<UGSBurnFXComponent> BurnFXComponent;

	/** Swapped in on detonation. Set-dressing debris is the level's job; this is the silhouette. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Visual")
	TObjectPtr<UStaticMesh> DestroyedMesh;

	/**
	 * Any child primitive carrying this component tag is treated as a window: a torch actor
	 * overlapping it lights the interior. Add a box per window in the Blueprint and tag it.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Windows")
	FName WindowComponentTag = TEXT("Window");

	/**
	 * Actor tag a projectile must carry to count as a lit torch. Keeps the goblin's own body,
	 * thrown loot and stray physics props from setting off the mill.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Windows")
	FName LitTorchActorTag = TEXT("GS.LitTorch");

	/** The fuse. Long enough to run, short enough to feel like a mistake if you don't. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Tuning")
	float DustBuildupSeconds = 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Mill|Tuning")
	float BuildupTickInterval = 0.1f;

	/** Detonation lights everything flammable nearby - this is how the mill takes the yard with it. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Mill|Tuning")
	float DetonationIgniteRadius = 1400.f;

	UPROPERTY(ReplicatedUsing = OnRep_Stage)
	EGSMillStage Stage = EGSMillStage::Intact;

	float BuildupElapsed = 0.f;
	FTimerHandle BuildupTimerHandle;
};
