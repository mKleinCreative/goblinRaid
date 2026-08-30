// Ground fire spawned by torch impacts: radial DoT with uniform friendly fire (design doc §4 -
// "fire doesn't check factions") plus delayed spread to nearby flammables. Reconstructed
// 2026-07-19 to match the surviving GSFireVolume.cpp exactly.
//
// 2026-07-30: DamageTick() was an empty TODO - fire had never damaged anything in this project.
// Now implemented through the real GAS path (UGSGE_FireDamage + UGSDamageExecCalculation), and a
// POOLED mode was added so AGSFieldFireObjective can park a few of these on its burn front instead
// of spawning one per cell (burn-types spec §3.1: "<=3 pooled AGSFireVolumes ... same class, same
// Damage.Fire + FriendlyFireScalar exec-calc path, so friendly fire and tuning stay uniform").
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSFireVolume.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UGameplayEffect;

UCLASS()
class GOBLINSIEGE_API AGSFireVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSFireVolume();

	/**
	 * Switch this volume into pooled mode BEFORE BeginPlay - i.e. between SpawnActorDeferred and
	 * FinishSpawning. A pooled volume is owned and repositioned by something else (the field), so
	 * it must not expire on a lifespan and must not run its own spread pass, which would duplicate
	 * the owner's propagation model.
	 *
	 * bInEnableSmoke added 2026-07-31 for Michael's objective 3 ("smoke must stay behind after the
	 * fire moves on"). AGSFieldFireObjective now owns its own smoke wisps parked on BURNT cells,
	 * which is the only way smoke can outlive the front - a plume attached to a pooled volume
	 * necessarily walks away with the volume. With the field's wisps running, leaving smoke on the
	 * volumes as well double-counts the plume exactly where the fire already is, so the field
	 * passes false. A torch-spawned (non-pooled) volume is unaffected and still smokes.
	 *
	 * bInEnableLight added 2026-07-31 (Q-33) - THE LIGHT COUNT IS NOW DECOUPLED FROM THE VOLUME
	 * COUNT, and that separation is the entire point of the parameter.
	 *
	 * MaxFireVolumes was raised from the spec's 3 to 10 on 2026-07-30 for a reason that is purely
	 * about FLAMES: "3 big volumes read as three tidy bonfires, not a field ablaze". Nothing about
	 * that argument applies to the point light. Burn-types spec §3.4 caps the Blaze stage at THREE
	 * pooled shadowless point lights, and every AGSFireVolume builds a UPointLightComponent
	 * unconditionally - so raising the volume cap silently raised the light cap with it. T-09
	 * confirms three fields on Tutorial_Island, which is up to 30 dynamic lights on a map whose
	 * lighting is meant to stay static at dusk.
	 *
	 * Ten flame patches with three lights between them looks very nearly identical to ten lit ones
	 * - the lights overlap heavily at the field's 170uu volume radius and 6x LightRadiusScale, so
	 * the spill is already pooled long before it reaches the ground - while costing a third of the
	 * dynamic lighting. The caller decides; see AGSFieldFireObjective::MaxLitVolumes.
	 */
	void ConfigurePooled(float InDamageRadius, bool bInEnableSmoke = true, bool bInEnableLight = true);

	/**
	 * Toggle this volume's point light AFTER BeginPlay (2026-07-31, Q-33).
	 *
	 * ConfigurePooled alone is not enough for the field, because the pooled volumes are
	 * REPOSITIONED, NOT RESPAWNED: a volume is spawned once and then walks the burn front for the
	 * rest of the raid. Slot ordering is stable but slot COUNT is not - the field grows and shrinks
	 * the pool as the front does - so the set of slots below MaxLitVolumes changes over a raid
	 * while every volume in it keeps whatever lit-ness it was born with. Without a live setter the
	 * cap would only ever be applied at the moment each volume happened to spawn.
	 *
	 * Re-running ApplyFireLight() was the alternative and was rejected: it never CLEARS
	 * FlickerTimerHandle on the way down (it only early-returns out of the enable path), so a
	 * volume switched off would keep a 20 Hz timer running for the rest of the raid doing nothing,
	 * and switching one back on would re-issue SetTimer on a handle that was already live.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void SetLightEnabled(bool bNewEnabled);

	/** Radius setter that keeps the collision sphere in sync. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void SetDamageRadius(float NewRadius);

	/**
	 * 0..1 "how established is the fire here". Drives FX scale and light brightness so a burn
	 * has an ARC instead of a constant: a lone new cell is a small flame, ground that is properly
	 * involved reaches full N_MeteorSpawn scale, and it drops away as cells burn out.
	 *
	 * Michael's note on the reference was specifically that the meteor look is the PEAK - so it
	 * has to be something the fire builds up to, not the state it spawns in.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire")
	void SetFireIntensity(float NewIntensity01);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetFireIntensity() const { return FireIntensity01; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetDamageRadius() const { return DamageRadius; }

	/** Total damage this volume has applied. Purely for verification/debug readouts. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire")
	float GetTotalDamageDealt() const { return TotalDamageDealt; }

protected:
	virtual void BeginPlay() override;

	void DamageTick();
	void SpreadTick();

	/** Applies one tick of fire damage to a single actor, if it has an ASC and is a legal target. */
	void ApplyFireDamageTo(AActor* Target);

	/** Loads FireSystem/SmokeSystem onto their components, scales them, and activates. */
	void ApplyFireFX();

	/**
	 * Recomputes SmokeBaseScale from the CURRENT DamageRadius and re-applies it. Called from
	 * ApplyFireFX and from SetDamageRadius, so a volume that is resized at runtime (the field
	 * repositions and resizes its pooled volumes) grows its plume to match instead of keeping a
	 * scale derived from whatever radius it happened to have at BeginPlay.
	 */
	void RefreshSmokeScale();

	/** Sets up the shadowless point light from LightColour/Intensity/RadiusScale. */
	void ApplyFireLight();

	/** Drives LightFlicker. Timer-based rather than actor tick to match the rest of this class. */
	void FlickerTick();

	FTimerHandle FlickerTimerHandle;
	float BaseLightIntensity = 0.f;

	/**
	 * Radius-derived plume scale, computed once in ApplyFireFX. SetFireIntensity multiplies the
	 * intensity arc on top of this instead of overwriting it - which is the bug that made smoke
	 * shrink back to native size on every intensity update.
	 */
	float SmokeBaseScale = 1.f;

	/** 0..1, driven by the owning field from local burn density. */
	float FireIntensity01 = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<USphereComponent> DamageSphere;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<UNiagaraComponent> FireFX;

	/**
	 * Drifting smoke. Half of what makes a reference napalm shot feel "overwhelming" is haze -
	 * the flames alone read as tidy campfires no matter how well tuned.
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<UNiagaraComponent> SmokeFX;

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<UNiagaraComponent> EmberFX;

	/**
	 * The orange spill onto surrounding geometry. This is the other half: in the reference, every
	 * rock and every soldier is lit orange, and that bounce is what makes fire feel like it owns
	 * the space rather than sitting in front of it. Shadowless by design (design doc: lighting
	 * stays static at dusk; the fire may not touch the skylight or cast shadow maps).
	 */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Fire")
	TObjectPtr<UPointLightComponent> FireLight;

	/**
	 * The flames. Defaults to NS_GS_SurfaceFire, resolved by soft path so a missing asset degrades
	 * to "fire that damages but isn't drawn" rather than failing to load the class.
	 *
	 * The system is authored for a ~140uu spawn radius; BeginPlay scales the component so a volume
	 * with a different DamageRadius still has flames that match the area it actually burns. Without
	 * that, the field's 320-radius volumes would show torch-sized flames in the middle of a fire
	 * three times wider.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	TSoftObjectPtr<UNiagaraSystem> FireSystem;

	/** Spawn radius the FireSystem was authored at - the basis for the scale correction above. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float FireSystemAuthoredRadius = 140.f;

	/**
	 * OFF by default now that we use a purpose-built marketplace system.
	 *
	 * The radius-based rescale existed to stretch a hand-tuned torch flame across a bigger volume.
	 * FireSystem (NS_Fire_Big as of 2026-08-29, formerly N_MeteorSpawn) is used at native scale -
	 * both had no verified authored-radius number to rescale against, so native is the only size
	 * either was ever confirmed to look right at. Turn on only for a system authored at
	 * FireSystemAuthoredRadius, with that radius actually known.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	bool bAutoScaleFXToRadius = false;

	/**
	 * Smoke scales INDEPENDENTLY of the flame, and that is the whole point (2026-07-31).
	 *
	 * Michael on the first field-burn recording: "there's no smoke and we can't tell it's supposed
	 * to be burnt." The smoke was never missing - bEnableSmoke is true and NS_FlameSmoke was
	 * spawning on every volume. It was being DROWNED. With bAutoScaleFXToRadius off (correct - the
	 * marketplace flame is authored at the size that looked right), smoke rendered at its native
	 * authored size inside a 320uu field volume, and SetFireIntensity then scaled it to 0.6-1.0 -
	 * so the plume was always SMALLER than the flames it came from.
	 *
	 * Real fire is the other way round: the plume is wider and taller than the fire, and it is what
	 * you actually see from across a valley. The GDD wants a razed objective to read from the
	 * treeline; that reads as a smoke column long before it reads as flame.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	bool bScaleSmokeToRadius = true;

	/** Spawn radius the smoke system was authored at - the basis for the radius correction. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float SmokeAuthoredRadius = 140.f;

	/**
	 * How much bigger than the burning footprint the plume reads. THE knob for "I can't tell it
	 * burned" - raise it before touching anything else. 1.0 = plume exactly covers the damage
	 * radius, which already looks thin; above 1 it billows past the fire the way real smoke does.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX", meta = (ClampMin = "0.1"))
	float SmokePlumeScale = 1.5f;

	/** Scale at zero intensity. The fire ramps from here up to 1.0 as ground becomes involved. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MinIntensityScale = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	TSoftObjectPtr<UNiagaraSystem> SmokeSystem;

	/** Rising sparks. VolcanoEnvironmentVFX ships this as its own system. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	TSoftObjectPtr<UNiagaraSystem> EmberSystem;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	bool bEnableEmbers = true;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	bool bEnableSmoke = true;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	bool bEnableLight = true;

	/** Warm orange. Kept well below white so it reads as firelight rather than a spotlight. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	FLinearColor LightColour = FLinearColor(1.f, 0.42f, 0.12f, 1.f);

	/** 9000 blew out the scene with 10 volumes lit at once - firelight should warm the ground,
	 *  not act as area lighting. Cut from 4800 to 2400 on the NS_Fire_Big swap (#358, 2026-08-29):
	 *  that value was tuned against N_MeteorSpawn's own authored brightness, and against the real
	 *  fire texture the same intensity reads as light overpowering flame - Michael watched it live
	 *  and called it "too much light not enough flame, it blows out the effect." Re-tune by eye if
	 *  a future flame swap changes the balance again; this number is relative to whatever
	 *  FireSystem currently is, not an absolute. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float LightIntensity = 2400.f;

	/** Multiplied by DamageRadius - firelight should reach well past the flames themselves. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float LightRadiusScale = 6.f;

	/** Flicker depth (0 = steady). Fire that doesn't flicker reads as an orange lamp. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightFlickerAmount = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float LightFlickerSpeed = 11.f;

	/** The GameplayEffect carrying UGSDamageExecCalculation. Defaults to UGSGE_FireDamage. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Damage")
	TSubclassOf<UGameplayEffect> FireDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamageRadius = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamagePerTick = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float DamageTickInterval = 0.5f;

	/** Only matters when the burn victim is another goblin/ally; enemies always take full damage
	 *  (design doc §4 friendly-fire knob). */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float FriendlyFireScalar = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float SpreadDelaySeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float SpreadRadius = 220.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float LifetimeSeconds = 10.f;

	/** False in pooled mode - the owning objective decides when this volume dies. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	bool bAutoExpire = true;

	/** False in pooled mode - the owner runs its own propagation and would double up. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	bool bSpreadToFlammables = true;

	/** Alarm contributed once at spawn. Pooled volumes are long-lived and shouldn't re-bill it. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Tuning")
	float AlarmOnSpawn = 1.5f;

	float TotalDamageDealt = 0.f;

	FTimerHandle DamageTickHandle;
	FTimerHandle SpreadTimerHandle;
};
