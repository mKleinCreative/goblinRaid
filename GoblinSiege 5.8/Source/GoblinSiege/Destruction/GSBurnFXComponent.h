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

	/**
	 * Char a DIFFERENT actor's meshes instead of this component's own owner.
	 *
	 * Written for AGSMillObjective (2026-08-30): the objective owns MillMesh/SailMesh, but both are
	 * empty placeholder components - the mill's actual visible geometry is a separate, dynamically-
	 * resolved StaticMeshActor placed on the level (see SinkTower's own geometry search). Without
	 * this, CacheMaterialInstances() finds zero populated material slots on the owner and char is
	 * driven correctly (SetBurnAmount ticks 0->1, logged and confirmed) while producing no visible
	 * effect at all - the "warning, not a crash" degrade path silently doing exactly what it is
	 * supposed to do, for an actor it was never actually wrong to warn about.
	 *
	 * Re-caches immediately, because by the time an owning actor's own BeginPlay body runs (after
	 * its `Super::BeginPlay()` call) this component's BeginPlay - and therefore its first
	 * CacheMaterialInstances() - has ALREADY run. A caller that resolves its real geometry actor at
	 * runtime (the mill can't know it at construction) has to be able to redirect after the fact,
	 * not just at construction time.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Fire|FX")
	void SetCharTargetActor(AActor* NewTarget);

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
	 * The "this place is burnt" marker. Soft path, same degrade-not-crash rule as
	 * AGSFireVolume::FireSystem: a missing/unresolvable asset costs the smoke, not the actor.
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

	/**
	 * Hard ceiling on the bounds-derived scale. 0 disables the clamp.
	 *
	 * There was a lower clamp (0.1) here and no upper one, and that asymmetry put a plume on screen
	 * at **481x** on 2026-09-09 - a single column of smoke taller than the sky box, which is most of
	 * what Michael's "still too many smoke issues" screenshot actually shows. The cause is fixed
	 * separately (the scale now comes from the INTACT footprint snapshotted at BeginPlay, not from
	 * bounds read mid-collapse while debris is scattering), so this is the backstop rather than the
	 * fix: it makes a runaway impossible regardless of what a future actor's bounds do, and logs
	 * loudly when it bites so the underlying bounds problem is visible rather than silently hidden.
	 *
	 * 6 is roughly "the biggest building on the map"; the median plume measured 1.86x.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder", meta = (ClampMin = "0.0"))
	float MaxSmolderScale = 6.f;

	/** Default ON: the razed state has to survive to raid end, or the scoreboard lies. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	bool bSmolderForever = true;

	/** Only read when bSmolderForever is false. 0 also means forever. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder")
	float SmolderDurationSeconds = 0.f;

	/**
	 * Global cap on simultaneously active smolder systems, across every actor wearing this component
	 * on the whole map - NOT per-building. Same reasoning as AGSBuildingObjective::MaxFireFX, but this
	 * component has no building-level coordinator to enforce a per-building cap through (it drives
	 * per-piece char independently of whatever objective, if any, owns the piece).
	 *
	 * Found live (2026-09-01), "the Inn specifically": a genuinely kitbashed (never mesh-merged, unlike
	 * the rest of the village) building with 440 individually-adopted pieces, each wearing its own
	 * UGSBurnFXComponent. bSmolderForever=true is correct design ("a razed place stays smoking for the
	 * rest of the raid") for an isolated haycart or granary, but at 440 pieces it meant up to 440
	 * permanent Niagara systems accumulating on ONE building - "each wall has its own emitter," visibly
	 * (a wall of individual smoke columns) and on the frame budget. A handful of small buildings across
	 * the map smoking forever is the intended picture; one building alone maxing out this budget is not.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder", meta = (ClampMin = "0"))
	int32 MaxGlobalSmolderFX = 40;

	/**
	 * Minimum world-space distance between two live smolder plumes, in unreal units. A piece that
	 * burns down within this range of an existing plume chars normally and simply adds no second
	 * column of smoke. 0 disables the rule and restores the pure first-come budget above.
	 *
	 * This is the spacing half of the same complaint MaxGlobalSmolderFX answered the population
	 * half of. Michael, 2026-09-09: "the Inn spawns essentially one of those per wall panel or prop
	 * it feels like ... limit the amount of smoke by proximity so it's never one giant cloud."
	 * A count-only budget cannot express that - 40 plumes spread over the village reads as a raided
	 * hamlet, and the same 40 stacked inside the Inn's footprint reads as one opaque grey blob AND
	 * starves every other building of smoke.
	 *
	 * 1500 is a house-ish footprint: the Inn's own AdoptRadius is 2523uu and an ordinary merged
	 * house is well under this, so the intended result is roughly one plume per building rather
	 * than one per wall panel. It is a look number, not a physical one - tune it by eye.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Fire|Smolder", meta = (ClampMin = "0.0"))
	float MinSmolderSpacing = 1500.f;

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

	/** Set via SetCharTargetActor(). Null (the default) means "char the owner", which is correct
	 *  for every drop-on-anything user of this component - only the mill currently overrides it. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CharTargetActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BurnMIDs;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SmolderFX;

	/** True once this instance has incremented the global smolder counter - so EndPlay decrements
	 *  exactly the instances that actually counted, not every component regardless of whether its
	 *  spawn was allowed or refused by MaxGlobalSmolderFX. */
	bool bCountedTowardGlobalSmolderCap = false;

	/** The owner's bounds as they were at BeginPlay, i.e. while it was still intact. Both the plume's
	 *  position and its scale come from these rather than from live bounds - see BeginPlay. */
	FVector IntactBoundsOrigin = FVector::ZeroVector;
	FVector IntactBoundsExtent = FVector::ZeroVector;
	bool bHasIntactBounds = false;

	/** Last value pushed. Never decreases on its own - char is a ratchet. */
	float CurrentBurnAmount = 0.f;

	/** Set on burn-down. Stops the drive timer from ever restarting off a stray re-ignite. */
	bool bCharLocked = false;

	FTimerHandle BurnFXTimerHandle;
	FTimerHandle SmolderStopHandle;
};
