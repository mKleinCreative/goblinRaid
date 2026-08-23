// Copyright Goblin Siege.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBowTimingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnBowDrawStarted, float, TraverseSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnBowDrawProgress, float, Position01, int32, Bounces);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnBowDrawEnded, bool, bLoosed);

/**
 * The bow's timing minigame: an indicator sweeps a coloured bar while the draw is held, and the
 * moment of release decides the arrow's damage.
 *
 * ---------------------------------------------------------------------------------------------
 * THIS COMPONENT EXISTS ONLY ON THE PLAYER, AND THAT IS THE WHOLE AI SAFETY STORY.
 *
 * `UGSGA_BowShot` is shared: BP_ErikaArcher's RangedAttackAbilityClass and the player's
 * BowShotAbilityClass both point at the same C++ class, and there is no Blueprint child to separate
 * them. So anything added INSIDE the ability would change every defender archer too.
 *
 * Putting the timing on a component the player pawn carries and AI archers do not means the gate is
 * structural rather than conditional - there is no `IsPlayerControlled()` branch anywhere in this
 * feature, and there is no way to accidentally give Erika a timing bar. `FireArrow` asks the avatar
 * for this component and falls back to a multiplier of 1.0 when it is absent.
 * ---------------------------------------------------------------------------------------------
 *
 * Shaped after two things this project already does: the heavy-attack charge rig on
 * AGSPlayerCharacter (hold -> 0..1 alpha -> BlueprintAssignable delegate -> HUD) and the interact
 * channel's Started/Progress/Ended delegate triple, which UGSPlayerHUDWidget already knows how to
 * consume.
 */
UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSBowTimingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBowTimingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the sweep. Safe to call twice - a second call while already drawing is ignored rather
	 *  than restarting, so an input repeat cannot reset the player's timing. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Bow")
	void BeginDraw();

	/**
	 * End the draw WITHOUT loosing - a weapon swap, a dodge, a guard break, death. Broadcasts
	 * OnBowDrawEnded(false) so the bar disappears, and leaves no quality behind for a later shot to
	 * pick up.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Bow")
	void CancelDraw();

	/**
	 * Sample the indicator where it stands and end the draw. Returns the damage multiplier.
	 *
	 * SAMPLED AT INPUT RELEASE, deliberately, not at the moment the arrow spawns. UGSGA_BowShot puts
	 * ReleaseDelaySeconds (0.08) between the two, and the honest moment is the one the player
	 * actually judged - charging them for 80ms of release recoil they cannot see would make a
	 * perfect shot feel stolen.
	 *
	 * Returns 1.0 if no draw was in progress, so a stray call can never zero a shot.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Bow")
	float ConsumeReleaseQuality();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	bool IsDrawing() const { return bDrawing; }

	/** 0..1 across the bar, whichever direction the sweep is travelling. The HUD does not need to
	 *  know about direction - the bar looks the same either way. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetPosition01() const { return Position01; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	int32 GetBounces() const { return Bounces; }

	/**
	 * The band layout, for the HUD material to draw a gradient whose red is the SAME red this
	 * component pays out on. Authoring the gradient by eye instead would guarantee they drift apart
	 * eventually, and the player would be aiming at a lie.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetRedCentre() const { return RedCentre; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetRedHalfWidth() const { return RedHalfWidth; }

	/** Half of OrangeFraction - the width of ONE orange band, which is what a symmetric gradient
	 *  needs. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetOrangeHalfWidth() const { return OrangeFraction * 0.5f; }

	/** The damage multiplier this position would earn, for a HUD that wants to preview it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetQualityAt(float InPosition01) const;

	/**
	 * What the last release earned, for UGSGA_BowShot::FireArrow to read.
	 *
	 * The ability runs AFTER the draw has ended - the character samples on input release, then
	 * activates - so the live sweep is already gone by the time the arrow spawns. This holds the
	 * verdict across that gap.
	 *
	 * Reset to 1.0 whenever a draw begins or is cancelled, so a shot the ability REFUSES (the fire
	 * interval, a blocking tag) can never leave a perfect score lying around for the next arrow to
	 * collect.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetLastReleaseQuality() const { return LastReleaseQuality; }

	/**
	 * The aim perturbation to add to the control rotation, read by UGSAimComponent::GetAimRotation.
	 * Zero while not drawing, and zero on the first pass - sway is the price of OVERholding, not of
	 * drawing at all.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	FRotator GetSwayOffset() const;

	/**
	 * HOW HARD THE BOW IS DRAWN, as a multiplier on the arrow's launch speed.
	 *
	 * This is the part the player can actually SEE while holding. A slow arrow falls further over the
	 * same distance, so a weak release lobs and a perfect one flies nearly flat - and because the
	 * predicted arc reads the same number every frame, the trajectory visibly straightens as the
	 * indicator climbs toward red. The damage curve rewards timing; this one EXPLAINS it, without a
	 * number on screen.
	 *
	 * Continuous, unlike GetQualityAt: 1.0 across the whole red band, falling to MinSpeedScale at
	 * either end of the bar. Damage has a deliberate discontinuity at the red edge because a bullseye
	 * should feel like a distinct reward; a matching jump in TRAJECTORY would just look like the arc
	 * glitching.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetSpeedScaleAt(float InPosition01) const;

	/** Live value for the aim arc. Returns MinSpeedScale before the sweep has moved - a bow at rest
	 *  is a bow at zero draw - and 1.0 when no draw is in progress, so nothing else is disturbed. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetCurrentSpeedScale() const;

	/** The speed scale the last release earned, held across the gap between input release and the
	 *  arrow spawning. Same lifetime rules as GetLastReleaseQuality. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Bow")
	float GetLastReleaseSpeedScale() const { return LastReleaseSpeedScale; }

	// ---------------------------------------------------------------------------- animation
	//
	// THE GOBLIN SET ONLY, and that is not an oversight. The player is always the goblin scout, and
	// this component only ever exists on the player - so the human archery montages are Erika's
	// business and live on UGSGA_BowShot instead, played only for an avatar that has no timing
	// component. Same structural split as the damage multiplier: no IsPlayerControlled() anywhere.
	//
	// Three assets rather than three sections of one, matching UGSGA_Horn - the shape is the same
	// event (attack, sustain, release) and drift between the two would be visible.
	//
	// All soft and all optional: a missing montage costs the animation, not the mechanic.

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> DrawMontage;

	/** Loops until the player releases. Its Default section is self-linked in the asset. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> HoldMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Bow")
	FGSOnBowDrawStarted OnBowDrawStarted;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Bow")
	FGSOnBowDrawProgress OnBowDrawProgress;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Bow")
	FGSOnBowDrawEnded OnBowDrawEnded;

protected:
	// ---------------------------------------------------------------------------- the sweep

	/** Seconds for one end-to-end pass. The indicator then reverses; it does not snap back. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sweep", meta = (ClampMin = "0.1"))
	float TraverseSeconds = 5.f;

	/**
	 * Where the perfect shot sits, as a fraction of the bar. 0.54 puts it at 2.7s into a 5s pass -
	 * Michael's number. Just past centre, so the gradient is very slightly asymmetric and the eye
	 * has something to aim at rather than "the middle".
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sweep", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RedCentre = 0.54f;

	/** Half the red band's width. 0.025 makes red 5% of the bar. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sweep", meta = (ClampMin = "0.001"))
	float RedHalfWidth = 0.025f;

	/** Orange's total share of the bar, split evenly either side of red. Yellow is whatever is
	 *  left - it is never authored directly, so the three bands cannot drift out of sync. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sweep", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float OrangeFraction = 0.40f;

	// ---------------------------------------------------------------------------- damage

	/**
	 * A perfect shot. FLAT, and deliberately discontinuous with the top of orange - the jump from
	 * 1.0 to 2.0 at the band edge is the reward for hitting a 5% window, not a rounding error in a
	 * ramp. If this is ever smoothed into the orange curve the mechanic stops having a bullseye.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Damage", meta = (ClampMin = "0.0"))
	float RedMultiplier = 2.f;

	/** Orange interpolates from this at its outer edge to OrangeInnerMultiplier against red. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Damage", meta = (ClampMin = "0.0"))
	float OrangeOuterMultiplier = 0.6f;

	/** 1.0: an orange shot is exactly the arrow's authored damage, so everything already tuned keeps
	 *  its meaning and the minigame adds a reward above and a penalty below. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Damage", meta = (ClampMin = "0.0"))
	float OrangeInnerMultiplier = 1.f;

	/** The worst a shot can be: released at the very ends of the bar. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Damage", meta = (ClampMin = "0.0"))
	float YellowOuterMultiplier = 0.25f;

	// ---------------------------------------------------------------------------- sway

	/** Degrees of wander at ONE bounce. Zero bounces is always perfectly steady. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sway", meta = (ClampMin = "0.0"))
	float SwayDegreesPerBounce = 1.6f;

	/**
	 * The launch speed of the weakest possible release, as a fraction of the arrow's authored speed.
	 *
	 * 0.40 rather than something tiny: at very low speeds the arc stops reading as "a weak shot" and
	 * starts reading as "the bow is broken", and an arrow that lands at the player's feet is not a
	 * lesson, it is a joke. This still roughly quadruples the drop over a given distance, which is
	 * plenty to make spam feel useless.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Damage", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MinSpeedScale = 0.40f;

	/** How fast the figure-eight travels. Slow on purpose: the player must be able to shoot THROUGH
	 *  a target as the aim drifts across it, which a fast wander turns into luck. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Bow|Sway", meta = (ClampMin = "0.0"))
	float SwayRateRadPerSec = 1.1f;

private:
	void Advance(float DeltaTime);
	void EndDraw(bool bLoosed);

	/** Plays a montage on the owning character, or does nothing if either is missing. */
	void PlayBowMontage(const TSoftObjectPtr<UAnimMontage>& Montage);
	void StopBowMontage();

	/** Draw -> Hold handoff. The draw clip is 1.03s and the hold loops after it; a timer is how the
	 *  horn does the same thing, rather than a montage-ended delegate that a cancel would race. */
	void HandOffToHold();
	FTimerHandle DrawHandoffTimer;

	/** What PlayBowMontage last started, so StopBowMontage stops the right thing rather than every
	 *  montage on the character - a hit reaction or a dodge must not be cancelled by a bow release. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveBowMontage;

	bool bDrawing = false;

	/** 0..1 position along the bar. */
	float Position01 = 0.f;

	/** +1 sweeping right, -1 sweeping left. Ping-pong, so the indicator never snaps. */
	float Direction = 1.f;

	/** Ends reached this draw. Drives sway amplitude; uncapped by ruling - holding forever is
	 *  self-defeating because nothing can be held on target, but the shot is always the player's to
	 *  take and the game never fires it for them. */
	int32 Bounces = 0;

	/** Seconds since the draw began, for the sway oscillator. */
	float DrawElapsed = 0.f;

	/** See GetLastReleaseQuality. */
	float LastReleaseQuality = 1.f;

	/** See GetLastReleaseSpeedScale. */
	float LastReleaseSpeedScale = 1.f;
};
