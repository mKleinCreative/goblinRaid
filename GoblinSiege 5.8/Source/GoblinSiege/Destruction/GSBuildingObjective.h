// A building that can be set alight - through a window, or across its roof.
// Written 2026-08-05 (Michael's flow: "a torch gets thrown into a window ... or onto the ceiling").
//
// ---------------------------------------------------------------------------------------------
// WHY THIS CLASS EXISTS AT ALL: there was no building.
//
// A house on L_Tutorial_Island is not a mesh and not an actor. It is 20-40 loose StaticMeshActors
// kitbashed together - floors of 12 triangles, walls, corners, beams, roof segments, windows -
// out of 150 distinct piece types and 1,413 instances across the map. Nothing owns a house, so
// "the building catches fire" had nothing to happen TO. This actor is the missing noun: it adopts
// the pieces standing inside its footprint and becomes the thing that burns, scores and completes.
//
// It is deliberately the same shape as AGSMarketObjective, which already solves "adopt a cluster of
// flammable props and complete at a fraction of them" - and it inherits that class's hard-won rule:
//
//     A CLUSTER OBJECTIVE'S ADOPT RADIUS MUST NOT EXCEED WHAT ITS SPREAD DISTANCE CAN TRAVERSE.
//
// The market spent a session unwinnable because it adopted 64 scattered stalls when fire could only
// ever reach ~30 of them. Adopting more pieces makes a building HARDER to burn, not richer, and
// nothing anywhere reports the shortfall. Hence AdoptRadius defaults tight and the placement script
// derives it from real geometry.
//
// ---------------------------------------------------------------------------------------------
// THE IGNITION RULE (the whole point of the feature)
//
// A stone-and-plaster house does not catch because you held a torch to its outside wall. Unlike the
// windmill (whose exterior-fire-immune rule was retired 2026-08-30, #361/#362 - it now takes a
// torch anywhere on its own geometry), a building still refuses its walls. It has exactly two ways
// in:
//
//   1. A WINDOW. Break it and the torch goes through. See UGSBreakableComponent.
//   2. THE ROOF. Thatch and beams, lit from above.
//
// Everything else is refused, and ContainsWorldLocation returns false for the walls so the torch's
// own objective lookup will not light a house by splashing its facade.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSBuildingObjective.generated.h"

class UGSFlammableComponent;
class UGeometryCollectionComponent;
class UGSCrumbleComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnBuildingIgnited);

/** How a building was lit - for barks, score, and (later) which door civilians flee out of. */
UENUM(BlueprintType)
enum class EGSBuildingIgnitionSource : uint8
{
	None,
	Window,
	Roof,
	Spread,
	Debug
};

UCLASS()
class GOBLINSIEGE_API AGSBuildingObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSBuildingObjective();

	/**
	 * Light the inside. THE entry point - a broken window calls this, the roof calls this, the
	 * debug command calls this. Idempotent: a building already alight ignores further torches.
	 *
	 * Server-only. Named to match AGSMillObjective::IgniteInterior, which is the same idea on the
	 * one building that already had it.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Building")
	void IgniteInterior(EGSBuildingIgnitionSource Source = EGSBuildingIgnitionSource::Debug);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	bool IsAlight() const { return bAlight; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	int32 GetPieceCount() const { return Pieces.Num(); }

	/** The pieces this building actually adopted. Not a UFUNCTION - TWeakObjectPtr does not cross
	 *  into Blueprint - but public because a diagnostic that RE-DERIVES the piece set is not a
	 *  diagnostic of this building. GS.Raid.BuildingStatus used to rebuild it as "anything flammable
	 *  within a flat 2500 uu of the pivot", which is both the pivot-vs-geometry bug commit 270d107
	 *  fixed in AdoptPieces and a radius unrelated to this building's AdoptRadius. */
	const TArray<TWeakObjectPtr<AActor>>& GetPieces() const { return Pieces; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	int32 GetBurntPieceCount() const { return BurntPieceCount; }

	/** How this building was first lit. Kept because it is the seam the fleeing-civilian work will
	 *  want (Michael, 2026-08-05: civilians flee burning buildings out the door - EARMARKED, not
	 *  built): a house lit through the roof should read differently from one lit at the window a
	 *  civilian is standing beside. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	EGSBuildingIgnitionSource GetIgnitionSource() const { return IgnitionSource; }

	/** Fires once, server-side, the moment the building goes up. The hook the civilian-flee work
	 *  binds to later. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Building")
	FGSOnBuildingIgnited OnBuildingIgnited;

	/** The building owning this piece, or null. Used by the torch and by breakable windows, and by
	 *  the same TActorIterator reasoning as AGSBurnObjectiveBase::FindObjectiveAtLocation:
	 *  buildings are tens of actors per level, not thousands. */
	static AGSBuildingObjective* FindBuildingOwning(const UObject* WorldContextObject, AActor* Piece);

	/**
	 * Is this piece part of the roof?
	 *
	 * Public and separate from the window path because the torch has to tell them apart: a window is
	 * broken and passed through, a roof is simply lit from above. Both are "entry", only one is a
	 * roof, and collapsing the two would make a torch bounced off a windowpane light the house
	 * without ever breaking it.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Building")
	bool IsRoofPiece(const AActor* Piece) const;

	/**
	 * Walls refuse fire; only the roof and windows let it in.
	 *
	 * Always false for a KITBASHED building - it owns a roof actor, so entry is decided by which
	 * piece was hit, and returning true here would let a torch light a house off its facade.
	 *
	 * For a MERGED building it is the roof TEST, because there is no roof actor to hit: true only
	 * when the point is inside the mesh's footprint and in the top RoofZoneFraction of its bounds.
	 */
	virtual bool ContainsWorldLocation(const FVector& WorldLocation) const override;

	/** Lights a MERGED building hit on its roof region. Does nothing for a kitbashed one, whose
	 *  ContainsWorldLocation is always false - a torch that splashes a wall must not light the
	 *  house, and that rule stays in one place. */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** Sweep the footprint and take ownership of the kit pieces inside it. */
	void AdoptPieces();

	/** Give every adopted piece a flammable component if it has none, so a kitbashed house can burn
	 *  at all. Mirrors UGSRaidLibrary::MakeActorFlammable, which exists because the market's stalls
	 *  had the same problem: plain StaticMeshActors cannot catch. */
	void EnsurePiecesFlammable();

	UFUNCTION()
	void HandlePieceBurnedDown();

	/**
	 * A burnt-out building comes down.
	 *
	 * Michael, 2026-08-24: "we break and destroy buildings with a torch ... on the destroyed state
	 * after being on fire ... they crumble into pieces like the statue." Completion IS the destroyed
	 * state for a building, so this is where the crumble belongs - not on any one piece burning out.
	 *
	 * No impulse: nothing shoved this house, it burnt through and gave way, so it goes straight down
	 * under its own weight. That is the difference between a building collapsing and an idol being
	 * hauled over, and it is the whole reason UGSCrumbleComponent takes the shove as a parameter
	 * rather than owning one.
	 */
	virtual void HandleCompleted() override;

	/**
	 * Release every adopted piece that has a fracture asset, and say how many did not.
	 *
	 * Silent per piece and loud once per building, on purpose. Almost every kit piece has no geometry
	 * collection today and that is the expected state, not an error - warning thirty times per house
	 * would train everyone to filter the category out, which is exactly how the old topple log came
	 * to announce success three times while the statue stood there unmoved. One line that says "8 of
	 * 34" is the instrument; thirty that say "no collection" is noise.
	 *
	 * A piece with no fracture asset no longer just stops where it stands (Michael, 2026-08-31: "not
	 * every building has their collapse mesh cached" - only 1 of 42 `SM_MERGED_House_*` meshes has a
	 * matching `GC_` today). See SpawnGenericRubbleFallback.
	 */
	void CrumblePieces();

	/**
	 * A piece with no matching GC_ fracture asset still has to visibly break - a house that "completes"
	 * as an objective while standing untouched reads as the feature not working. This is deliberately
	 * NOT a fracture (a Geometry Collection bakes in the specific geometry it was built from, so a
	 * real fracture from a DIFFERENT mesh would render the wrong house's rubble) and, as of #393, NOT
	 * physics either: hides the piece, plays a dust/sound cue, destroys it.
	 *
	 * TWO EARLIER VERSIONS, BOTH REVERTED LIVE (2026-08-31):
	 *   v1 hid+disabled-collision+Destroy()ed with a dust burst - Michael watched the house vanish
	 *   with the dust hanging in mid-air where it used to stand: "This is not an acceptable outcome."
	 *   v2 replaced that with a plain-rigid-body topple (SetSimulatePhysics + an off-center impulse,
	 *   same pattern as UGSInteractableComponent::ApplyCollapse) - this broke two different ways: on a
	 *   MERGED house (one big mesh) enough impulse to visibly rotate it read as the building launching
	 *   into the air ("why in gods green earth did you think the entire house popping up would be a
	 *   good idea"), and on individual KITBASHED trim pieces it failed outright because they ship with
	 *   'Use Complex Collision As Simple', which the physics engine cannot simulate on at all - every
	 *   call logged a warning and did nothing, leaving pieces with no collision response at all
	 *   ("it has no collision, so I can just walk through it").
	 * #393's bulk fracture generation covers whole-mesh MERGED houses, not individual KITBASHED trim
	 * pieces (SM_House_Roof_01_*, SM_House_Wall_5x4_*, etc.) - so this is very much NOT rarely reached
	 * for a kitbashed building. CORRECTION, 2026-09-01: a kitbashed building can adopt hundreds of
	 * pieces (measured: 440 on one building alone) and NONE of them have a matching GC_, so
	 * CrumblePieces calls this once PER PIECE on completion. Confirmed live via `stat dumpframe`: 577
	 * simultaneous N_PebbleDust instances, ~70ms of game-thread time from Niagara particle-collision
	 * checks alone (2308 of them in one frame) - Michael's "it's really laggy right now" / "fire
	 * causes the issue" was this, not the merged-house collapse piece count #393 already capped.
	 * bPlayFX (see CrumblePieces, which owns the per-building cap - MaxRubbleFXPerBuilding, the same
	 * pattern this class already uses for MaxFireFX) lets every piece still be correctly
	 * hidden/removed while only a bounded few actually spawn a dust/sound cue - a building disappearing
	 * with 4 dust puffs instead of 440 reads identically to the player and costs nothing close to the
	 * same.
	 */
	void SpawnGenericRubbleFallback(AActor* Piece, bool bPlayFX) const;

	/**
	 * Find this piece's fracture asset by naming convention, attaching it if it is not already there.
	 *
	 * A house on this map is a placed StaticMeshActor out of a modular kit - there is no Blueprint to
	 * hand-wire a collection onto, and 55 merged houses across 41 distinct meshes is not a hand job.
	 * So the mesh name IS the lookup: SM_MERGED_House_Small_03 -> GC_MERGED_House_Small_03 in
	 * CrumbleCollectionFolder. Same reasoning as every other filter in this class - the kit carries no
	 * metadata to ask, and a naming convention is the only signal the art actually carries.
	 *
	 * Returns null, silently, when no collection has been authored for that mesh. That is the normal
	 * state of this kit and is what lets houses be fractured ONE AT A TIME instead of all 41 before
	 * anything works.
	 *
	 * The collection is created HIDDEN and Chaos_Object_Static - dormant. A collection left at its
	 * default Chaos_Object_Dynamic falls over and shatters itself at level start, which #192 recorded
	 * as the feature working before Michael pointed out he had never touched it.
	 */
	AActor* SpawnCollectionProxy(AActor* Piece) const;

	/** Where fracture assets live. Mesh SM_Foo resolves to GC_Foo in here. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Crumble")
	FString CrumbleCollectionFolder = TEXT("/Game/Destruction");

	// Collapse shape, handed to each piece's UGSCrumbleComponent. Defaults are what Michael and I
	// tuned live in PIE on 2026-08-26 against a 32-chunk merged house; see that component's header
	// for what each one buys and what it looked like when it was wrong.
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Crumble", meta = (ClampMin = "0", ClampMax = "8"))
	int32 CollapseClusterPasses = 3;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Crumble", meta = (ClampMin = "0", ClampMax = "16"))
	int32 CollapseShoveCount = 7;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Crumble", meta = (ClampMin = "0.0"))
	float CollapseShoveMagnitude = 5000000.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Crumble", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollapseInwardRatio = 0.6f;

	/** A piece caught. Spawns the close-up flame on it, up to MaxFireFX. */
	UFUNCTION()
	void HandlePieceIgnited();

	/** Attach fire to a piece that is currently burning, respecting the cap. */
	void SpawnFireFXOn(AActor* Piece);

	void RecomputeCompletion();

	/** True if this piece reads as a way in (roof or window) rather than a wall. Name-based, like
	 *  the market's stall filter - the kit has no metadata to ask, and a mesh naming convention is
	 *  the only signal the art actually carries. */
	bool IsEntryPiece(const AActor* Piece) const;

	UFUNCTION()
	void OnRep_Alight();

	// ------------------------------------------------------------------ tuning

	/**
	 * Footprint. Pieces inside this are the building.
	 *
	 * Kept tight on purpose - see the class comment. The placement script sizes it from the actual
	 * cluster rather than trusting this default, which exists only so a hand-placed building is
	 * sane out of the box.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning", meta = (ClampMin = "100.0"))
	float AdoptRadius = 1200.f;

	/** Substrings that mark a piece as a way IN rather than a wall (windows and roof). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> EntryNameFilters;

	/** Substrings that mark a piece as ROOF specifically - the half of "entry" a torch can light
	 *  from above without breaking anything. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> RoofNameFilters;

	/**
	 * Pieces that are INSIDE the building - interior walls, floors, stairs, ceiling beams.
	 *
	 * Michael, 2026-08-06: "I think you're getting caught up and confused on interiors." He was
	 * right, and it was the whole problem. 28% of the kit is interior, a tavern with many rooms and
	 * a balcony read as 422 pieces, and the floors sat at different heights - which is what split
	 * every multi-storey house into one objective per storey.
	 *
	 * These are still ADOPTED and still burn, so fire spreads through a building properly. They are
	 * simply not COUNTED toward completion: a player judges a house by its outside, and burning a
	 * cellar floor they cannot see should not be what stands between them and the objective.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> InteriorNameFilters;

	/** True if this piece is interior - adopted and flammable, but not scored. */
	bool IsInteriorPiece(const AActor* Piece) const;

	/** Adopted pieces that are NOT interior: the denominator RecomputeCompletion actually divides by.
	 *  Exists so the BeginPlay diagnostic and the live score cannot describe different numbers. */
	int32 CountShellPieces() const;

	/** Substrings a piece's mesh must match to be adopted at all. Empty adopts anything with a
	 *  static mesh inside the radius, which drags in barrels and market tables. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> PieceNameFilters;

	/**
	 * Reject a candidate piece whose own bounds reach further than AdoptRadius x this. A wall is
	 * smaller than the building it belongs to; a landscape or a map-wide foliage actor is not.
	 *
	 * 2.0 is deliberately loose - a big roof or a long wall can legitimately out-reach a tight adopt
	 * radius, and this is a backstop against world-sized actors, not a tight fit. The case it exists
	 * for measured 63,076 against an AdoptRadius of 2,523: twenty-five times over, not two.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Adoption", meta = (ClampMin = "1.0"))
	float OversizePieceRejectRatio = 2.f;

	/** Alarm the moment the house goes up. A burning building is unmissable; louder than a field. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	float AlarmOnIgnite = 15.f;

	/**
	 * Spread radius handed to every piece this building adopts. 0 leaves the component's own default.
	 *
	 * WHY BUILDINGS GET THEIR OWN NUMBER: UGSFlammableComponent defaults to 450uu, which is right for
	 * a fence catching a haycart and far too short for a village. Measured on Tutorial Island, the
	 * nearest PIECE-to-piece gap between neighbouring village houses has a median of 745uu - so at
	 * 450 only about a third of houses can ever reach a neighbour, and a torched village stops at the
	 * first gap. At 1200 one torch took 21 of the village's 36 houses in 90 seconds and the fire
	 * travelled 88 metres, which is the picture Michael asked for: "it'd be quicker to burn house
	 * blocks".
	 *
	 * Deliberately NOT a change to the component's default - fences, haycarts and market stalls keep
	 * the tighter 450/600 they were tuned with, so raising the reach of houses cannot silently turn
	 * every hedgerow into a 12-metre firebomb.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning", meta = (ClampMin = "0.0"))
	float PieceSpreadRadius = 1200.f;

	// ------------------------------------------------------------------ visuals
	//
	// A house that burns invisibly is a house that did not burn, as far as the player is concerned.
	// This is the GDD's burn mandate applied to buildings: "anything burnable chars black +
	// smoulders". Three layers, because they answer three different questions:
	//
	//   char + smoulder  - is this thing damaged?      (per piece, free via UGSBurnFXComponent)
	//   surface fire     - is this thing ON FIRE now?  (per piece, capped)
	//   smoke column     - is something burning over THERE? (one per building, reads from range)
	//
	// The column is the one the raid actually needs. GDD 2.1 wants a fire to be findable from the
	// treeline - "lights the map for a mile" - and per-piece flames are invisible past a hundred
	// metres.

	/** Close-up flame, attached per burning piece. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX")
	TSoftObjectPtr<UNiagaraSystem> FireSystem;

	/** The distant tell. One per building, at its centre, for as long as it burns. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX")
	TSoftObjectPtr<UNiagaraSystem> SmokeColumnSystem;

	/** SpawnGenericRubbleFallback's one-shot burst for a piece with no fracture asset. C++-defaulted
	 *  to the project's existing generic debris dust system (same reasoning as
	 *  AGSTorchProjectile::FireVolumeClass - a reference left to a content folder is one bad merge
	 *  from going missing, and null here would make the fallback invisible AND silent). */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX")
	TSoftObjectPtr<UNiagaraSystem> GenericRubbleFXAsset;

	/** SpawnGenericRubbleFallback's impact sound. C++-defaulted to the project's existing large-debris
	 *  cue, same reasoning as GenericRubbleFXAsset. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX")
	TSoftObjectPtr<USoundBase> GenericRubbleSoundAsset;

	/**
	 * How many pieces may show flames at once.
	 *
	 * Capped because a 66-piece house would otherwise light 66 Niagara systems in one second, on a
	 * render thread already measured at 15.4ms of its 16.67ms budget (perf ticket 002). Eight reads
	 * as a burning house; sixty-six reads as a dropped frame.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX", meta = (ClampMin = "0"))
	int32 MaxFireFX = 8;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraComponent>> ActiveFireFX;

	/**
	 * Same reasoning as MaxFireFX, for CrumblePieces' generic-rubble-fallback dust/sound - see
	 * SpawnGenericRubbleFallback's header comment. A large KITBASHED building can have hundreds of
	 * pieces with no fracture asset; spawning N_PebbleDust for every single one measured live at 577
	 * simultaneous Niagara instances and ~70ms of game-thread time from particle-collision checks
	 * alone. Every piece is still hidden/removed regardless of this cap - only the dust/sound burst is
	 * bounded.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX", meta = (ClampMin = "0"))
	int32 MaxRubbleFXPerBuilding = 6;

	/**
	 * Map-wide ceiling on simultaneous rubble-dust bursts, across every building at once. 0 disables.
	 *
	 * MaxRubbleFXPerBuilding above is per BUILDING, which is not a cap at all when the whole village
	 * comes down together - 67 buildings x 6 is ~400. Measured in a razed village on 2026-09-09:
	 * **141 simultaneous N_PebbleDust instances costing 8.3 ms of game thread, 7.98 ms of it Niagara
	 * particle COLLISION.** That was the single largest identified item in a 46 ms frame.
	 *
	 * Exactly the mistake #395 found in the smolder FX and fixed with a global budget; this is the
	 * same shape of fix for the other per-piece Niagara system in this module. If a third one ever
	 * appears, it wants one of these too before it ships.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|FX", meta = (ClampMin = "0"))
	int32 MaxGlobalRubbleFX = 24;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> SmokeColumn;

	// ------------------------------------------------------------------ state

	/** Adopted kit pieces. Weak: a piece destroyed by other means simply drops out. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Pieces;

	UPROPERTY()
	TArray<TWeakObjectPtr<UGSFlammableComponent>> PieceFlammables;

	/**
	 * How much of a MERGED building's height counts as roof.
	 *
	 * Only consulted when the building owns no roof actor. A third is the eaves line on this kit's
	 * houses; lower would let a torch into an upstairs wall, higher would demand a near-vertical drop
	 * onto the ridge and make the throw feel broken.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning",
		meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float RoofZoneFraction = 0.34f;

	/**
	 * Substrings marking a piece that IS a whole building - walls, roof and windows in one mesh.
	 *
	 * Such a piece owns no roof ACTOR, so its roof is the top RoofZoneFraction of its own bounds
	 * instead. Asked per PIECE, never per building: 14 of this map's merged houses also adopt a stray
	 * roof tile from a neighbouring shed, and treating the two as exclusive left those houses with no
	 * way in at all.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Building|Tuning")
	TArray<FString> MonolithicNameFilters;

	/** True if this piece is a whole building baked into one mesh - see MonolithicNameFilters. */
	bool IsMonolithicPiece(const AActor* Piece) const;

	/** Set at adoption, so completion has a stable denominator even as pieces are destroyed. */
	int32 InitialPieceCount = 0;

	int32 BurntPieceCount = 0;

	/** Warn-once latch for "no shell left to score". A member, not a file-scope static, because this
	 *  is per-building state on a level-placed actor - unlike the projectile latches in #030/#034,
	 *  which were members on actors spawned fresh every shot and so could never latch at all. */
	bool bWarnedNoShell = false;

	UPROPERTY(ReplicatedUsing = OnRep_Alight)
	bool bAlight = false;

	EGSBuildingIgnitionSource IgnitionSource = EGSBuildingIgnitionSource::None;
};
