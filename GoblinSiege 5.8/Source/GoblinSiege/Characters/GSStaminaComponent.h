// The stamina pool. Added 2026-08-06.
//
// Until now stamina was a float on BP_GSPlayerCharacter, which meant only Blueprint could read it -
// so swimming, drowning, dodge and block could not cost stamina however much the design wanted them
// to. This component is the single owner; Blueprint calls into it rather than holding the number.
//
// NOT a GAS attribute, deliberately. Health/Armor/MoveSpeedMultiplier are attributes because they are
// modified BY OTHER ACTORS through GameplayEffects - armour reduces incoming damage, a slow debuff
// scales speed. Stamina is spent by its own owner, every frame, in response to input. Routing that
// through the effect pipeline buys the mitigation machinery nothing and pays for it in per-tick
// GameplayEffect churn. Revisit if something ever needs to drain another goblin's stamina.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSStaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnStaminaChanged, float, NewStamina, float, MaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnExhausted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnRecovered);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSStaminaComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Spend a fixed amount. Returns false and spends NOTHING if the pool cannot cover it.
	 *
	 * All-or-nothing on purpose: a vault that takes your last 3 stamina and then fails to clear the
	 * wall is worse than a vault that refuses. Callers should treat false as "the move did not
	 * happen" rather than trying again with less.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stamina")
	bool TryConsume(float Cost);

	/**
	 * Sustained drain, in stamina per second. 0 stops the drain and lets regen resume.
	 *
	 * A RATE rather than a per-tick subtraction, because that is what makes the GDD's "idle hold
	 * drains zero" fall out for free: hanging on a wall calls SetDrainRate(0) and the same code path
	 * handles it. A per-tick model would need an explicit "are we moving" branch in every caller.
	 *
	 * LAST WRITER WINS - there is no stacking. Sprint, climb and swim are mutually exclusive states,
	 * so a stack would only ever mask a bug where two of them think they are active at once.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stamina")
	void SetDrainRate(float PerSecond);

	/** Refill to full, e.g. on respawn. Clears exhaustion. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stamina")
	void ResetToFull();

	/**
	 * Hold the pool still: no drain, no regen. The wall.
	 *
	 * `SetDrainRate(0)` is NOT enough for this, and that is the whole reason this exists. Zero drain
	 * means regen resumes, so hanging on a wall would slowly refill you - and then any building is
	 * climbable in stages and the meter stops gating verticality at all.
	 *
	 * Michael's ruling (2026-08-07): hanging is FREE but not RESTFUL. You can stop and look around
	 * without being punished for it, but you cannot park on a wall to reset a long climb, so a tall
	 * building stays one commitment. That is the GDD's "idle hold drains zero" without Genshin's
	 * idle-drain, which is its second most-complained-about behaviour.
	 *
	 * Suppression does not touch the regen DELAY timer - releasing the wall resumes regen from
	 * wherever the delay had got to, rather than restarting it and punishing you twice.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stamina")
	void SetRegenSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	bool IsRegenSuppressed() const { return bRegenSuppressed; }

	/** The rate currently being drained, per second. 0 when nothing is draining. Exposed so a caller
	 *  can tell "idle on the wall" from "not climbing" without tracking that state a second time. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	float GetDrainRate() const { return DrainRate; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	/** 0..1, for a progress bar. Guards MaxStamina <= 0 rather than returning a NaN into the HUD. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	float GetStaminaNormalised() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }

	/**
	 * True from the moment the pool empties until it refills past RecoverFraction.
	 *
	 * A latch, not `Stamina <= 0`. Without it a player at zero regains 1 stamina next frame, sprints
	 * for that frame, empties again, and the whole thing stutters. The BP version already worked this
	 * way and the behaviour is preserved deliberately.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stamina")
	bool IsExhausted() const { return bExhausted; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stamina")
	FGSOnStaminaChanged OnStaminaChanged;

	/** The pool just hit zero. Climb listens to let go; swimming listens to drown. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stamina")
	FGSOnExhausted OnExhausted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stamina")
	FGSOnRecovered OnRecovered;

protected:
	/** Seeded from BP_GSPlayerCharacter's MaxStamina so feel does not change on the day this lands. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Stamina|Tuning", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	/** Per second, while nothing is draining. BP value was 25/s ("4s recovery"). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Stamina|Tuning", meta = (ClampMin = "0.0"))
	float RegenRate = 25.f;

	/**
	 * Seconds after the last spend before regen starts. 0 reproduces the old behaviour exactly.
	 *
	 * Defaulted to 0 on purpose: the BP had no regen delay, and this pass is meant to move stamina
	 * without changing how it feels. It exists because "sprint, tap the brakes, sprint again" is the
	 * obvious exploit and this is the dial that closes it - but turning that dial is a balance
	 * decision, not part of a port.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Stamina|Tuning", meta = (ClampMin = "0.0"))
	float RegenDelaySeconds = 0.f;

	/** Fraction of max the pool must reach before exhaustion lifts. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Stamina|Tuning",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecoverFraction = 0.25f;

private:
	void SetStamina(float NewValue);

	/** Replicated so a client HUD is not guessing. Not ReplicatedUsing - the delegate fires locally
	 *  from SetStamina on the server and from OnRep on clients, via the same helper. */
	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	float Stamina = 100.f;

	UFUNCTION()
	void OnRep_Stamina();

	UPROPERTY(Replicated)
	bool bExhausted = false;

	float DrainRate = 0.f;
	float SecondsSinceSpend = 0.f;

	/** Set while attached to a wall. Not replicated: the server owns the pool and drives the number,
	 *  and a client that guessed at suppression would only diverge between corrections. */
	bool bRegenSuppressed = false;
};
