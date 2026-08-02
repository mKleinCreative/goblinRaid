// Fire has to leave a MARK. Michael watched a PIE recording of the field fire and ruled twice on
// 2026-07-31: "We need a blueprint we can add to the material of items that will darken them based
// off of their interaction with fire", and "the fire needs to keep burning or at least smoldering
// (with a small fire or maybe just a lot of smoke) to denote a place has been burnt."
//
// Both notes are the same complaint: the raid has no memory. Flames play, the burn timer runs out,
// and the haycart is the same clean prop it was before the goblins ever touched it - so a player
// sweeping back through a village they already razed cannot read their own progress off the world.
// Destruction is a pillar (GDD §11.0), not a ten-second animation.
//
// This component is the receipt. It drives ONE scalar - GS_BurnAmount - across every material slot
// of every mesh on its owner, fed from UGSFlammableComponent's burn progress, and it never rewinds:
// a doused building stays scorched at whatever char it reached, a burned-down one stays black for
// the rest of the raid. On burn-down it hands off to a persistent smolder system so a razed place
// still reads as razed from across the valley at dusk.
//
// Extended 2026-07-31 with GROUND SCORCH: on burn-down it also splats the world burn mask
// (UGSBurnMaskSubsystem) at its own footprint, so a razed granary marks the dirt it stood on and
// not only its own mesh. That fell out for free when the burn mask moved from one render target
// per wheat field to one for the whole world - a world-space mask does not care what put the mark
// there. See SplatGroundScorch and bSplatGroundScorch.
//
// Deliberately drop-on-anything. A missing flammable component is a warning, not a crash, and
// pushing a scalar at a material that has never heard of GS_BurnAmount is a safe no-op in UE - so
// the whole prop kit can wear this today and the materials can opt in one at a time.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBurnFXComponent.generated.h"

class UCurveFloat;
class UGSBurnMaskSubsystem;
class UGSFlammableComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSBurnFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBurnFXComponent();

	/**
	 * Scrub the char by hand, 0..1. For designers previewing the look and for the GSDBG burn
	 * commands. Does not start or stop the drive timer - a live fire will simply overwrite this on
	 * its next FX tick, which is what you want when scrubbing something that is currently alight.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire|FX")
	void SetBurnAmount(float NewValue01);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Fire|FX")
	float GetBurnAmount() const { return CurrentBurnAmount; }

	/**
	 * Start the lingering smolder now, regardless of how the burn was driven (2026-08-01, Q-34).
	 *
	 * The flammable path calls this automatically on burn-down; drivers that bypass the flammable
	 * component entirely - the mill's dust fuse pushes SetBurnAmount directly - get no
	 * HandleBurnedDown and therefore char without ever smoking unless they call this themselves.
	 * Michael's ruling is explicit that burnt means "turns black AND smoulders", so the manual
	 * drivers need this door. Idempotent: a second call while a smolder is alive does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire|Smolder")
	void SpawnSmolder();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleIgnited();

	UFUNCTION()
	void HandleExtinguished();

	UFUNCTION()
	void HandleBurnedDown();

	/** Walks every UMeshComponent on the owner and makes a MID per populated material slot. */
	void CacheMaterialInstances();

	/** Timer-driven, not TickComponent - matches the rest of this module. */
	void BurnFXTick();

	/** Runs raw burn progress through BurnResponseCurve, or passes it through if the curve is null. */
	float EvaluateResponse(float RawProgress01) const;

	/** Pushes the value at every cached MID. Missing parameters are a no-op, by design. */
	void ApplyToMaterials(float Value);

	/** Only used when bSmolderForever is false. */
	void StopSmolder();

	/**
	 * Marks the GROUND under this prop as burnt, via UGSBurnMaskSubsystem (2026-07-31).
	 *
	 * This is the free half of the world-mask re-architecture. The mask was built so a wheat field
	 * could publish its burn as a texture the crop material samples by world position - and a
	 * world-space mask does not care what put the mark there. So a granary that burns down now
	 * scorches the ground it stood on, at no cost beyond one splat, and the razed village reads as
	 * razed even where the burnt thing itself has stopped drawing attention to itself.
	 *
	 * It is deliberately NOT part of the char ratchet: char is a material parameter on THIS actor,
	 * scorch is a mark on the world, and conflating them would mean a doused building that froze
	 * at 40% char also left a 40% ground mark it never earned. Burn-DOWN only.
	 */
	void SplatGroundScorch();

	// ------------------------------------------------------------------ char

	/**
	 * The single contract between this component and the art. A material opts into being burnable
	 * simply by having a scalar of this name wired into its base colour / roughness darkening; one
	 * that doesn't ignores us entirely, so attaching this broadly costs nothing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	FName BurnAmountParam = FName("GS_BurnAmount");

	/**
	 * Optional shaping of burn progress -> char. Linear char looks wrong: real material blackens
	 * early and then stops changing, so an ease-out curve here sells it. Null is fine - raw linear.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	TObjectPtr<UCurveFloat> BurnResponseCurve;

	/** How often the char is pushed while burning. Cheap: a handful of scalar sets. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX")
	float FXTickInterval = 0.1f;

	/**
	 * Where the char locks on burn-down. 1.0 is fully blackened; drop it for materials whose full
	 * char reads as a hole rather than a ruin.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|FX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FinalBurnAmount = 1.0f;

	// --------------------------------------------------------------- smolder

	/**
	 * The "this place is burnt" marker. Soft path exactly like AGSFireVolume::FireSystem: the asset
	 * does not exist yet, and a missing one has to degrade to "no smoke plus a warning" rather than
	 * a load failure that takes the owning actor down with it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	TSoftObjectPtr<UNiagaraSystem> SmolderSystem;

	/** Author-side multiplier applied on top of the bounds-derived scale below. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	float SmolderScale = 1.0f;

	/**
	 * Footprint radius the smolder system is authored for. A haycart and a windmill both wear this
	 * component, so the plume is scaled by owner bounds / this - same correction as
	 * AGSFireVolume::FireSystemAuthoredRadius. Set to 0 to disable auto-scaling entirely.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	float SmolderAuthoredRadius = 150.f;

	/** Default ON: the razed state has to survive to raid end, or the scoreboard lies. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	bool bSmolderForever = true;

	/** Only read when bSmolderForever is false. 0 also means forever. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	float SmolderDurationSeconds = 0.f;

	// --------------------------------------------------------- ground scorch

	/**
	 * Leave a burn mark on the GROUND when this prop burns down (2026-07-31). See
	 * SplatGroundScorch.
	 *
	 * On by default because a burnt granary that leaves clean grass under it is the same complaint
	 * that started all of this - "the raid has no memory" - just moved down a metre. Turn it off
	 * for anything that burns while not standing on ground the player can see: a wall torch, a
	 * roof-mounted prop, anything inside a building's floor.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Scorch")
	bool bSplatGroundScorch = true;

	/**
	 * Multiplier on the owner's own footprint radius, so the scorch reads as slightly larger than
	 * the thing that made it - fire licks out past a building's footprint, and a mark exactly the
	 * size of the object looks like a decal someone forgot to scale.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Scorch", meta = (ClampMin = "0.1"))
	float ScorchRadiusScale = 1.25f;

	/** Floor on the scorch radius, in unreal units, for props whose bounds are tiny. The world mask
	 *  is ~3 m per texel on Tutorial_Island, so anything much below this marks a single texel or
	 *  none at all - see UGSBurnMaskSubsystem::BurnMaskResolution. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Scorch", meta = (ClampMin = "0.0"))
	float MinScorchRadius = 200.f;

	// ----------------------------------------------------------------- state

	UPROPERTY(Transient)
	TObjectPtr<UGSFlammableComponent> FlammableComp;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BurnMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SmolderFX;

	/** Last value pushed. Never decreases on its own - char is a ratchet. */
	float CurrentBurnAmount = 0.f;

	/** Set on burn-down. Stops the drive timer from ever restarting off a stray re-ignite. */
	bool bCharLocked = false;

	FTimerHandle BurnFXTimerHandle;
	FTimerHandle SmolderStopHandle;
};
