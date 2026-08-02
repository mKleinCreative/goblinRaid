// The wheat field: the most visible arson in the game (design doc §6.3, burn-types spec §3).
// Fire races row to row across a crop grid, doused cells become FIREBREAKS that block spread but
// can still be re-lit by a direct torch (decision 26 + spec §3.3), the burn may JUMP to adjacent
// flammables at the field's edge, and completion is measured in cells - >=70% (Q-03, ruled
// 2026-07-21; this was cited as "decision 32" until 2026-07-31, which is a different ruling
// entirely - Q-32 is the one-burn-of-each-type win condition).
// Written 2026-07-28. Revised 2026-07-30 to match the approved spec on three counts:
//   1. Doused is a first-class state, not a synonym for Unburnt - see EGSFieldCellState.
//   2. Grid defaults are the hamlet generator's real numbers (5x8 @ 270), not invented ones.
//   3. Cell state is REPLICATED as a packed array with RepNotify, not fired as unreliable
//      multicasts - a co-op client needs authoritative state, both to render and to be corrected.
//
// Cells are DATA, not actors. A field is dozens-to-hundreds of cells; spawning an actor per cell
// would cost more than the rest of the raid put together.
//
// Revised again 2026-07-31, against Michael's three objectives for the wheat fire:
//   1. "Fire must leave a mark."       -> the burn mask. Cells cannot hold per-object material
//      instances (they are array entries, not actors), so the burn is published as a texture that
//      the crop material samples by world position.
//
//      RE-ARCHITECTED LATER THE SAME DAY (2026-07-31): the mask is no longer owned here. It moved
//      to UGSBurnMaskSubsystem - ONE render target for the whole world instead of one per field.
//      The per-field version was written assuming crop is static mesh ACTORS inside the field's
//      grid box; verified live in the editor, it is not. Tutorial_Island's wheat is painted
//      foliage (~275,000 SM_VillageWheat instances inside InstancedFoliageActors, and zero plain
//      static mesh actors using a wheat mesh), and foliage components are per-ISLAND, not
//      per-field - so a MID on one covers every instance at once and could only ever sample one
//      of the three fields' masks. Read the subsystem header for the full reasoning. What is left
//      here is the field's half of the deal: it tells the subsystem where its cells are and what
//      state they are in, and knows nothing about render targets.
//   2. "Fire must spread smoothly and grow as it goes, like a wave."
//      -> HEAT DIFFUSION replaces Bernoulli percolation. Ignition becomes determined by exposure
//      rather than decided by a coin flip, over an eight-neighbour kernel, weighted by each
//      burning cell's own burn arc so a broad front genuinely pushes harder than a lone cell.
//      See HeatPerSecondFromNeighbour and FuelVariance for the full reasoning.
//   3. "Smoke must stay behind after the fire moves on."
//      -> SmokeWisps, a second pool assigned to BURNT cells. Smoke could never persist while it
//      was a component on the pooled fire volumes, because those track the front by design.
#pragma once

#include "CoreMinimal.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "GSFieldFireObjective.generated.h"

class AGSFireVolume;
class UGSBurnMaskSubsystem;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;

/**
 * Cell lifecycle. Doused is deliberately NOT the same as Unburnt:
 *
 *   Unburnt  - dry crop. Catches from spread, catches from a torch.
 *   Burning  - alight, accumulating burn time, trying to light neighbours.
 *   Burnt    - ash. Never relights. The only state that counts toward completion.
 *   Doused   - wet crop. A FIREBREAK: spread cannot cross it. A direct torch still can relight it.
 *
 * Collapsing Doused into Unburnt (the original implementation) is what makes dousing feel useless:
 * a saved cell surrounded by fire is simply re-caught a tick later, so the player sees no result
 * from the one defensive verb the town has.
 */
UENUM(BlueprintType)
enum class EGSFieldCellState : uint8
{
	Unburnt,
	Burning,
	Burnt,
	Doused
};

USTRUCT()
struct FGSFieldCell
{
	GENERATED_BODY()

	EGSFieldCellState State = EGSFieldCellState::Unburnt;

	/**
	 * Accumulated burn time. Deliberately NOT reset when doused, so a re-torched cell finishes
	 * faster - the same "rewards re-torching what the defenders saved" rule the flammable
	 * component uses.
	 *
	 * CLAMPED on re-ignition since 2026-07-31, though, to the start of the intensity plateau. The
	 * head start is kept; what is removed is a relit cell resuming inside its ramp-OUT, at a
	 * quarter intensity, with two seconds left and no ability to push the front back across the
	 * firebreak it just retook. See IgniteCell.
	 *
	 * Also note this is NOT monotonic on a client - OnRep_CellStates zeroes it on every ignition
	 * so the ember arc restarts. Anything that needs a value that only rises wants CharHigh.
	 */
	float BurnSeconds = 0.f;

	/** Time spent wet. Doused crop dries out and becomes flammable again - see DousedDryOutSeconds. */
	float DousedSeconds = 0.f;

	/** Set once a burnt cell has done its outward jump attempt, so it only tries once. */
	bool bHasJumped = false;

	/**
	 * Accumulated ignition heat, 0..1; the cell catches the moment this reaches 1.0 (2026-07-31,
	 * objective 2 - "fire must spread smoothly and grow as it goes, like a wave").
	 *
	 * This replaces the per-tick coin flip that used to decide spread. The point of accumulating
	 * rather than rolling is that ignition time becomes DETERMINED by exposure instead of decided
	 * by luck, so two cells with the same neighbours light at nearly the same moment - which is
	 * the entire difference between a front and a scatter.
	 *
	 * Server-only, deliberately not replicated: a client that knows every cell's STATE has
	 * everything it needs to render, and a float per cell per update would cost more than the whole
	 * packed state array it would ride alongside.
	 */
	float Heat = 0.f;

	/**
	 * Per-cell fuel density, rolled ONCE when the grid is allocated and never touched again.
	 * See FuelVariance for the reasoning - stable fuel is what gives the front texture instead
	 * of noise, and re-rolling it every tick is precisely the mistake this change undoes.
	 */
	float FuelScale = 1.f;

	/**
	 * Monotonic stamp of WHEN this cell became ash: 0 = never burnt, higher = more recent. Set
	 * from AGSFieldFireObjective::BurntSequenceCounter as the cell enters Burnt, on the server and
	 * (via OnRep_CellStates) on clients.
	 *
	 * Exists for the smoke wake, which needs "most recently razed" and cannot get it from state
	 * alone - Burnt is terminal, so every burnt cell looks identical afterwards and a wisp pool
	 * with no ordering information can only ever sit on whichever cells finished first. See
	 * WispReseedSeconds. A plain counter rather than a timestamp because it only ever needs to be
	 * COMPARED, and a counter means clients need no synchronised clock to agree on the ordering.
	 */
	int32 BurntSequence = 0;

	/**
	 * High-water mark of char, 0..1, for the burn mask's R channel (2026-07-31).
	 *
	 * The header's channel contract promises R is MONOTONIC and never decreases, and crop
	 * materials are told to rely on that. Deriving R from BurnSeconds broke it: OnRep_CellStates
	 * zeroes the client-side cosmetic BurnSeconds whenever a cell enters Burning, so a cell that
	 * was doused and then re-torched visibly UN-CHARRED on every client - freshly scorched crop
	 * turning golden again. Splitting the two lets BurnSeconds stay free to restart the G ember
	 * arc, which is what it is for on a client, while char keeps its own ratchet.
	 *
	 * Only ever assigned through FMath::Max, and pinned to 1 when the cell becomes Burnt.
	 */
	float CharHigh = 0.f;

	/**
	 * How much char this cell has actually DEPOSITED into the burn mask so far, 0..1, in
	 * uncompensated per-cell units (2026-07-31).
	 *
	 * CharHigh says how charred the cell IS; this says how much of that has already been paid to
	 * the accumulator. They are different questions because the mask's R is a DEPOSIT channel:
	 * splatting CharHigh again does not set R to CharHigh, it adds CharHigh on top of whatever is
	 * already there.
	 *
	 * WHY IT EXISTS: HandleCellChangedForFX used to splat CharHigh on EVERY state transition, on
	 * top of the per-tick slices CosmeticTick was already depositing. A cell that burned, was
	 * doused, dried out and was re-torched therefore paid its char three or four times over,
	 * saturating R long before the cell was actually ash and erasing the ramp the slices exist to
	 * draw. With this, both paths deposit AGAINST the same running total - CosmeticTick adds its
	 * slice, the Burnt transition tops up the remainder to exactly 1 - and the cell's lifetime
	 * contribution to R is exactly 1.0 however many times it changes state.
	 *
	 * Only ever raised, like CharHigh, and for the same reason: R is contractually monotonic and
	 * a deposit cannot be taken back.
	 */
	float CharDeposited = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnFieldCellChanged, int32, CellIndex, EGSFieldCellState, NewState);

UCLASS()
class GOBLINSIEGE_API AGSFieldFireObjective : public AGSBurnObjectiveBase
{
	GENERATED_BODY()

public:
	AGSFieldFireObjective();

	/**
	 * A torch landing in the crop. This is DIRECT ignition, so it relights Doused firebreaks as
	 * well as dry crop. Out-of-bounds locations are rejected.
	 */
	virtual void IgniteAtLocation(const FVector& WorldLocation) override;

	/** Grid footprint test (plus VerticalTolerance), so the torch knows this field owns the impact. */
	virtual bool ContainsWorldLocation(const FVector& WorldLocation) const override;

	/**
	 * The bucket brigade's verb. Turns Burning AND Unburnt cells within Radius into Doused
	 * firebreaks; Burnt cells are left alone, because ash never re-wets.
	 * Town Option C (decision 16): the brigade is effective at the EDGES only, so callers should
	 * pass edge positions - this function does not itself enforce that, the BT does.
	 *
	 * PRE-WETTING was added 2026-07-31 (Q-35). This used to convert Burning cells only, which made
	 * the brigade decorative: a firebreak has to span a full column to stop a Moore-kernel front,
	 * and under heat diffusion a whole column is essentially never alight at the same moment, so
	 * the one defensive verb in the game could not build the one thing it exists to build. Wetting
	 * crop ahead of the fire is also what a bucket line actually does. Bounded by the existing
	 * 25 s DousedDryOutSeconds rather than by any new rule - see the .cpp for the full reasoning.
	 *
	 * @return the number of cells actually CHANGED (not the number in range).
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Field")
	int32 DouseAtLocation(const FVector& WorldLocation, float Radius);

	/** Fraction of cells fully burnt. The brigade's >25% smoke-draw gate reads this. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	float GetBurntFraction() const;

	/** Instantly dries every Doused cell back to Unburnt. Debug convenience so the dry-out rule can
	 *  be observed without waiting DousedDryOutSeconds. Returns cells dried. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Field")
	int32 DryAllDousedCells();

	/** Firebreak coverage - how much of the field the defenders have successfully wetted. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	float GetDousedFraction() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	int32 GetCellCount() const { return Cells.Num(); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	int32 GetBurningCellCount() const { return BurningCellCount; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	int32 GetDousedCellCount() const { return DousedCellCount; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	FVector GetCellWorldLocation(int32 CellIndex) const;

	/** Which cell a world position maps to. Exposed so callers can report WHY an ignition did
	 *  nothing (ash never relights) instead of assuming it worked. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	int32 GetNearestCellIndex(const FVector& WorldLocation) const { return FindNearestCell(WorldLocation); }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	EGSFieldCellState GetCellState(int32 CellIndex) const
	{
		return Cells.IsValidIndex(CellIndex) ? Cells[CellIndex].State : EGSFieldCellState::Unburnt;
	}

	/** Centroid of currently-burning cells - where the pooled fire volumes and the smoke column
	 *  want to sit (spec §3.1). Returns the actor location when nothing is alight. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	FVector GetBurningCentroid() const;

	/**
	 * "How involved is this cell", 0..1 - THE single definition of that, used by everything
	 * (2026-07-31). Returns 0 for any cell that is not Burning.
	 *
	 * Shape is a trapezoid over the cell's own burn: it ramps up over the first
	 * IntensityRampInFraction of CellBurnSeconds, holds at full, and falls away over the last
	 * IntensityRampOutFraction.
	 *
	 * Three separate systems used to answer this question three different ways - the volumes
	 * measured local burning DENSITY, the heat model had no notion of it at all, and the FX layer
	 * had none either. That is why nothing lined up: the flames peaked at a different moment than
	 * the spread pushed hardest. One curve now drives all three:
	 *   - how much heat this cell radiates into its neighbours (objective 2: the front accelerates
	 *     because established cells push harder than freshly-lit ones);
	 *   - the burn mask's G channel, i.e. ember glow under the crop (objective 1);
	 *   - the pooled AGSFireVolume's SetFireIntensity, i.e. flame scale and light (objective 2).
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Field")
	float GetCellIntensity01(int32 CellIndex) const;

	// GetBurnMaskRT / GetFieldOrigin / GetFieldWorldSize / RefreshCropMaterialBindings were all
	// REMOVED on 2026-07-31 when the mask became world-scale. There is no field-scoped rectangle
	// any more, so publishing one would be publishing a lie: crop materials sample
	// GS_BurnMaskOrigin / GS_BurnMaskSize, which describe the whole level and come from
	// UGSBurnMaskSubsystem. Anything that wants the render target asks the subsystem
	// (UGSBurnMaskSubsystem::Get(this)->GetMaskRT()); anything that wants to re-bind crop spawned
	// at runtime calls its RefreshCropBindings(), which is now a single world-wide call instead of
	// one per field.

	/**
	 * Cosmetic hook, fired on server AND clients as cell state changes.
	 *
	 * This is the change-notification that makes the spec's single array-fed GPU emitter efficient:
	 * rebuild the Niagara position array when this fires, rather than re-uploading every tick. It
	 * is also the right hook for one-shot ignition accents (a small flare as a cell catches), which
	 * are transient and pooled and so don't threaten the effect-instance budget.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Field")
	FGSOnFieldCellChanged OnFieldCellChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void DrawDebugState() const override;

	/** Sizes Cells from Rows/Columns. Safe to call repeatedly; OnRep can arrive before BeginPlay. */
	void EnsureGridAllocated();

	void CellTick();

	/**
	 * @param bDirectIgnition true for a torch (relights Doused firebreaks), false for spread
	 *        (blocked by them). This flag is the entire firebreak mechanic.
	 */
	void IgniteCell(int32 CellIndex, bool bDirectIgnition);

	void SetCellState(int32 CellIndex, EGSFieldCellState NewState);
	void TryJumpToAdjacentFlammables(int32 CellIndex);

	/** Spawns/repositions/retires the pooled damage volumes to follow the burn front. */
	void UpdateDamageVolumes();

	/**
	 * Spawns/repositions the smoke wisps that sit on BURNT cells (2026-07-31, objective 3).
	 * Sibling of UpdateDamageVolumes, but deliberately a separate pool: the damage volumes track
	 * the FRONT and the wisps track the WAKE, and those are different sets.
	 *
	 * Called from CosmeticTick, NOT from CellTick, and this is load-bearing rather than tidy. The
	 * wisps are bare runtime UNiagaraComponents; runtime-created components do not replicate, so
	 * anything created only on the authority is invisible to every client. Driving them from
	 * CellTick meant that on a dedicated server nobody saw smoke at all, while the one machine
	 * with no renderer did all the ground traces and asset loads. CosmeticTick already runs on
	 * server and clients and is already skipped on a dedicated server, which is exactly the
	 * lifecycle this needs. A client has every cell's Burnt state via ReplicatedCellStates, and
	 * Burnt state is the only input here.
	 */
	void UpdateSmokeWisps();

	int32 FindNearestCell(const FVector& WorldLocation) const;
	bool IsEdgeCell(int32 CellIndex) const;
	void RecountCellStates();

	/**
	 * Drops a point onto whatever geometry is under it. Factored out of UpdateDamageVolumes on
	 * 2026-07-31 so the smoke wisps snap identically - a wisp floating 1300uu above the ground on
	 * a slope is exactly as wrong as a fire volume doing it, and two copies of the trace would
	 * drift apart. Returns the input unchanged when bSnapVolumesToGround is off or nothing is hit.
	 */
	FVector SnapPointToGround(const FVector& InLocation) const;

	// ---- cosmetic layer: runs on clients too --------------------------------------------------

	/**
	 * The field's cosmetic clock. Renamed from BurnMaskTick on 2026-07-31 when the mask moved to
	 * UGSBurnMaskSubsystem - the old name described work this function no longer does, and three
	 * things it DOES do are load-bearing and easy to lose in a refactor:
	 *
	 *   1. the CLIENT-SIDE cosmetic BurnSeconds clock. A client never runs CellTick, so without
	 *      this its BurnSeconds sits at 0 forever and GetCellIntensity01 - which drives the mask's
	 *      ember channel and any client-side flame arc - is permanently flat.
	 *   2. the CharHigh ratchet on the client side. OnRep_CellStates zeroes BurnSeconds on every
	 *      re-ignition, so char has to be high-watermarked separately or a doused-then-re-torched
	 *      cell visibly un-chars on every client.
	 *   3. UpdateSmokeWisps, which must run on every machine that draws (see UpdateSmokeWisps).
	 *
	 * It also re-publishes the TRANSIENT mask channels and deposits char: the world mask decays G
	 * and B every flush (they cannot be erased on an additive canvas - see
	 * UGSBurnMaskSubsystem::FlushSplats), so a live ember or a wet cell has to be re-splatted at
	 * this cadence or it fades out. That is the same reason the old RedrawBurnMask ran on a steady
	 * timer while anything burned; only the mechanism changed.
	 */
	void CosmeticTick();

	/** Bound to our own OnFieldCellChanged so the mask is driven by the delegate that fires on the
	 *  SERVER AND ON CLIENTS, rather than by CellTick, which is authority-only. */
	UFUNCTION()
	void HandleCellChangedForFX(int32 CellIndex, EGSFieldCellState NewState);

	UFUNCTION()
	void OnRep_CellStates();

	// ------------------------------------------------------------------ grid shape

	/**
	 * Defaults are the hamlet generator's actual field: 5x8 wheat at ~270 spacing (gen_farm).
	 *
	 * All three grid-shape properties are REPLICATED, COND_InitialOnly (2026-07-31). They look
	 * like pure level-design data, but clients read them - GetCellWorldLocation and everything the
	 * cosmetic layer does with it, which now means every splat position and splat radius the field
	 * hands the burn-mask subsystem - and a field SPAWNED at runtime by the hamlet generator would
	 * otherwise leave every client running the class defaults against a server with a different
	 * shape, marking the wrong ground. See GetLifetimeReplicatedProps.
	 */
	UPROPERTY(EditAnywhere, Replicated, Category = "GoblinSiege|Field|Grid", meta = (ClampMin = "1"))
	int32 Rows = 5;

	UPROPERTY(EditAnywhere, Replicated, Category = "GoblinSiege|Field|Grid", meta = (ClampMin = "1"))
	int32 Columns = 8;

	/** World units per cell, matching the generator's ~270 cm wheat spacing. */
	UPROPERTY(EditAnywhere, Replicated, Category = "GoblinSiege|Field|Grid")
	float CellSize = 270.f;

	/** How far above/below the field plane still counts as "in the crop" for ContainsWorldLocation.
	 *  Generous enough that a torch bouncing off a slope still lights the wheat. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Grid")
	float VerticalTolerance = 600.f;

	// ------------------------------------------------------------------ burn tuning

	/**
	 * Spec §1 said ~8s per cell. Raised to 22 on 2026-07-30 after a look pass: at 8s the whole
	 * field was ash inside a minute, so the fire never got to sit and feel overwhelming. The
	 * reference look depends on flame PERSISTING on ground it has already taken.
	 *
	 * This also slows the front, because a cell has to burn a while before neighbours catch.
	 *
	 * ClampMin added 2026-07-31: this is a divisor in GetCellIntensity01 and a term in the H_peak
	 * relation, and at or near 0 the whole burn arc degenerates.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "1.0"))
	float CellBurnSeconds = 22.f;

	/** Simulation step. ClampMin added 2026-07-31 - at 0 the repeating timer never fires and the
	 *  entire cell simulation silently stops, which is indistinguishable from the fire being
	 *  broken and takes a debugger to find. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.05"))
	float CellTickInterval = 0.25f;

	/**
	 * The burn ARC, as fractions of CellBurnSeconds: a cell's output ramps 0->1 over the first
	 * IntensityRampInFraction of its burn, holds at full, then falls 1->0 over the last
	 * IntensityRampOutFraction. Read GetCellIntensity01 for what consumes it.
	 *
	 * This trapezoid is where "grows as it goes" (objective 2) comes from, and it costs no
	 * special-case code anywhere. Because heat output is multiplied by it, a cell that caught two
	 * seconds ago barely warms the crop ahead of it while a fully involved one pushes at full
	 * strength - so a broad established front advances measurably faster than a lone new cell.
	 * The fire accelerates because it is BIGGER, which is the actual physical reason fires
	 * accelerate, rather than because a rate constant was scheduled to increase.
	 *
	 * Ramp-out is nearly twice ramp-in on purpose: crop catches fast and dies back slowly, and the
	 * long tail is what keeps ember glow (the mask's G channel) alive behind the front instead of
	 * snapping to black the instant a cell finishes.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityRampInFraction = 0.2f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityRampOutFraction = 0.35f;

	/**
	 * HEAT DIFFUSION - the spread model, replacing Bernoulli percolation (2026-07-31).
	 *
	 * Michael: "there's no consistency to how the fire is spreading." The root cause was here.
	 * What this class did until today was roll FRand() <= chance against four orthogonal
	 * neighbours every tick. That is percolation, and percolation has three properties that are
	 * all wrong for a crop fire, none of which any amount of retuning could fix:
	 *   - ignition times are geometrically distributed, so the variance is enormous. One
	 *     neighbour catches in a second, the one beside it takes twenty. There is no front,
	 *     only a scatter of blobs that eventually join up.
	 *   - four-neighbour spread can grow only a DIAMOND, whatever the bias values.
	 *   - the rate is constant. A field half ablaze pushes exactly as hard as one lit cell, so
	 *     the fire never builds.
	 *
	 * Heat accumulation fixes all three at once. Each tick, every burning cell pushes heat into
	 * its eight neighbours; a neighbour ignites when its accumulated Heat reaches 1.0. Ignition
	 * time becomes DETERMINED by exposure rather than decided by luck, so cells with comparable
	 * exposure light at comparable moments - and that, not any FX work, is what makes a burn read
	 * as a wave.
	 *
	 * The threshold is hard-coded at 1.0 rather than exposed, precisely so this property reads
	 * directly as "fraction of an ignition delivered per second by ONE fully involved orthogonal
	 * neighbour".
	 *
	 * THE TUNING ARITHMETIC (corrected 2026-07-31). What used to be documented here was
	 *     seconds to ignite from N full neighbours = 1 / (N * HeatPerSecondFromNeighbour)
	 * and that formula is how the unshippable defaults got shipped. It ignores three things that
	 * all subtract from the delivered total: decay running every tick on the receiving cell, the
	 * wind scale (which is BELOW 1 for most of the compass), and the emitter's own burn arc, which
	 * means a burning cell spends its ramp-in and ramp-out radiating less than full. A neighbour
	 * does not deliver Rate for CellBurnSeconds; it delivers Rate * ArcDuty for CellBurnSeconds,
	 * against a cell that is bleeding off Decay the whole time. The real relation:
	 *
	 *   H_peak = Rate * WindScale * FuelScale * (ArcDuty * CellBurnSeconds) - Decay * CellBurnSeconds
	 *      where ArcDuty = 1 - IntensityRampInFraction/2 - IntensityRampOutFraction/2  (= 0.725 default)
	 *
	 *   Spread requires H_peak > 1.0 for the WORST case you want to propagate:
	 *      a lone crosswind orthogonal neighbour at minimum fuel (FuelScale = 1 - FuelVariance).
	 *
	 *   At the shipped values: 0.105 * 1.0 * 0.75 * 15.95 - 0.008 * 22 = 1.08  -> spreads from any cell.
	 *
	 * Under the OLD defaults that same worst case came out at 0.87 against a threshold of 1.0, so
	 * the only neighbour that could ever ignite was the one directly downwind (windscale 1.6) - a
	 * torch dropped on the downwind column simply burned out alone. The margin above is
	 * deliberately thin (1.08, not 2.0): a wide margin makes every cell light at nearly the same
	 * moment and the field flashes over instead of advancing as a front.
	 *
	 * Across the default 5x8 field this puts the front at the far edge in roughly 50-80 s, in a
	 * burning band about 4 cells deep - the band is deep because CellBurnSeconds is 22 s and a
	 * cell takes only a few seconds to light its neighbour, so most of the field's width is alight
	 * at once. That IS the "fire that gets to sit" CellBurnSeconds was raised for on 2026-07-30.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.001"))
	float HeatPerSecondFromNeighbour = 0.105f;

	/**
	 * Heat bled off every non-burning cell, per second.
	 *
	 * Without decay this model has a fatal failure mode rather than a cosmetic one: a cell one
	 * square from a fire that burned out keeps every bit of heat it ever received, forever. Over a
	 * long raid the accumulated near-misses carry the whole crop past 1.0 and the field
	 * self-ignites with nothing touching it. Decay is also what lets a firebreak actually hold -
	 * the cells behind it cool off while the front in front of it dies.
	 *
	 * Keep it well below HeatPerSecondFromNeighbour (about a thirteenth of it here). Push it close
	 * and the fire cannot outrun its own cooling: the front stalls one cell out from wherever it
	 * started and the objective becomes unreachable.
	 *
	 * Lowered from 0.02 to 0.008 on 2026-07-31. Decay is subtracted for the WHOLE of a neighbour's
	 * CellBurnSeconds, not just while the receiving cell is near threshold, so at 22 s per cell
	 * the old value ate 0.44 of the 1.0 needed - nearly half the ignition budget - and that on its
	 * own is most of why the shipped defaults could not spread. See the H_peak relation in
	 * HeatPerSecondFromNeighbour: Decay * CellBurnSeconds is a term in it, and it scales with how
	 * long cells burn. If CellBurnSeconds is ever raised again, this has to come down with it.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0"))
	float HeatDecayPerSecond = 0.008f;

	/**
	 * Weight applied to heat crossing to a DIAGONAL neighbour. 0.7071 is 1/sqrt(2): the diagonal
	 * neighbour is that much further away, so it gets that much less.
	 *
	 * Using the Moore (8) neighbourhood instead of von Neumann (4) is not a refinement, it is the
	 * fix for the diamond. The set of cells reachable from a point source under 4-neighbour spread
	 * is a diamond by construction and no tuning value rounds it off. Eight neighbours with
	 * distance weighting give a front that curves - which is the difference between "the fire is
	 * spreading" and "the fire is spreading in a rhombus".
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DiagonalHeatScale = 0.7071f;

	/**
	 * Directional wind, replacing AlongRowBias.
	 *
	 * Heat into a neighbour is scaled by 1 + WindHeatBias * dot(unit offset to neighbour, wind),
	 * so the bias is CONTINUOUS around the compass rather than a single multiplier applied to the
	 * two column neighbours. This matters for the same reason the Moore neighbourhood does: a
	 * two-value bias stretches the diamond, a dot product produces an oval front leaning downwind
	 * - directionality you can read at a glance without it looking like a lattice artefact.
	 *
	 * The scale is floored at 0.05 rather than 0, so upwind crop still creeps. A fire that cannot
	 * spread upwind at all leaves a dead-straight edge behind its origin, which reads as a bug
	 * rather than as wind.
	 *
	 * ClampMax lowered from 0.95 to 0.70 on 2026-07-31. "Creeps" was aspirational: the floor keeps
	 * the RATE positive but says nothing about whether it beats decay, and it does not have to.
	 * At 0.95 the directly-upwind scale is 0.05, which delivers 0.105 * 0.05 * 0.75 * 15.95 = 0.06
	 * over a whole neighbour's burn against 0.008 * 22 = 0.18 of decay - net negative, so upwind
	 * heat never accumulates at all and every cell behind the torch is PERMANENTLY unignitable.
	 * That is worse than the dead-straight edge this floor was added to avoid, because it also
	 * caps the reachable burnt fraction and can put the 70% completion rule out of reach (Q-03,
	 * ruled 2026-07-21). 0.70 keeps upwind scale at 0.30, which clears decay with room to spare.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0", ClampMax = "0.70"))
	float WindHeatBias = 0.6f;

	/** Wind heading in GRID space, not world: 0 = +column axis, 90 = +row axis. Grid space so a
	 *  field rotated to follow a hillside keeps the wind running down its own rows rather than
	 *  cutting across them the moment a designer rotates the actor. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning")
	float WindDirectionDeg = 0.f;

	/**
	 * Per-cell fuel density spread, rolled ONCE per cell at grid allocation as
	 * FRandRange(1 - FuelVariance, 1 + FuelVariance) and never re-rolled. This is the key design
	 * point of the 2026-07-31 change, and it is worth being explicit about because the obvious
	 * implementation is the wrong one.
	 *
	 * A real crop is uneven, and the tempting way to express that is to jitter the spread roll
	 * every tick. That is exactly what the old percolation model did, and it is what Michael was
	 * looking at when he said the spread had no consistency. Re-rolled luck carries no
	 * information: the same cell is unlucky, then lucky, then unlucky again, so the front has no
	 * shape from one second to the next and the eye reads it as noise.
	 *
	 * Stable per-cell fuel gives the opposite result from the same amount of randomness. The field
	 * has a fixed, invisible texture, so the front is RAGGED BUT PREDICTABLE - it bulges through
	 * the rich patches and drags in the thin ones, in the same places every time, and raggedness
	 * that is consistent reads as terrain rather than as chaos.
	 *
	 * Applied to the RECEIVING cell (how readily this crop takes heat), not the emitting one.
	 * 0 = a perfectly uniform field, front advances as a clean arc.
	 *
	 * Raised from 0.15 to 0.25 on 2026-07-31, alongside the ignition-rate correction. This is a
	 * VISIBLE change, not a compensating one: with the rate fixed, the worst-case fuel cell is the
	 * number the rate is solved against (see H_peak in HeatPerSecondFromNeighbour, which uses
	 * FuelScale = 1 - FuelVariance = 0.75), so widening the spread widens the gap between the rich
	 * patches and the thin ones without making any cell unignitable. 0.15 was too tight to read at
	 * a glance - the front came out very nearly a clean arc, which is the look this property
	 * exists to break up. Push it much past 0.3 and the thin cells start lagging far enough that
	 * the front stops reading as one thing.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float FuelVariance = 0.25f;

	/**
	 * Seconds a Doused cell stays wet before drying back to Unburnt. 0 = firebreaks are permanent.
	 *
	 * Non-zero matters for more than realism. With permanent firebreaks a competent brigade can
	 * make the field mathematically impossible: dousing 23 of 40 cells caps reachable burnt at 42%,
	 * under the 70% threshold, forever - and the spec (§3.2) explicitly says doused cells must not
	 * make the objective unwinnable. Drying out keeps firebreaks tactically real (they stop THIS
	 * front, now) without letting them permanently delete the objective.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Tuning", meta = (ClampMin = "0.0"))
	float DousedDryOutSeconds = 25.f;

	// ------------------------------------------------------------------ damage (spec §3.1)

	/**
	 * The field does NOT spawn a fire volume per cell. It keeps at most MaxFireVolumes pooled
	 * AGSFireVolumes parked on the burn front and repositions them as the front moves.
	 *
	 * Reusing AGSFireVolume rather than rolling bespoke damage is the spec's explicit call: same
	 * class, same Damage.Fire exec-calc path, same FriendlyFireScalar - so friendly fire and damage
	 * tuning stay uniform across every fire in the game, and a spreading field fire cooks horde
	 * goblins for free (GDD §2.5).
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage")
	bool bSpawnDamageVolumes = true;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage")
	TSubclassOf<AGSFireVolume> FireVolumeClass;

	/**
	 * Raised from the spec's 3 to 10 on 2026-07-30 after a look-reference pass.
	 *
	 * The spec capped this at 3 to avoid 40 Niagara components, and that reasoning still holds -
	 * but 3 big volumes read as three tidy bonfires, not as a field ablaze. The reference (a
	 * napalm strike settling) is MANY separate flame patches scattered over the ground. Ten small
	 * sources hit that read while staying an order of magnitude under per-cell.
	 *
	 * Damage cost per volume is one overlap query every 0.5 s, so this is cheap; the real cost is
	 * 10 Niagara components + 10 unshadowed point lights. Drop it back if profiling complains.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxFireVolumes = 10;

	/**
	 * How many of those volumes get a POINT LIGHT (2026-07-31, Q-33). The rest run flames, embers
	 * and damage with the light switched off.
	 *
	 * MaxFireVolumes and the light count were the same number until today, purely because
	 * AGSFireVolume builds a UPointLightComponent unconditionally - nobody ever decided they should
	 * be coupled. And the argument that raised MaxFireVolumes to 10 on 2026-07-30 is an argument
	 * about FLAMES only: "3 big volumes read as three tidy bonfires, not a field ablaze". The
	 * reference look is many separate flame patches, and a flame patch does not need its own light
	 * to read as one. Burn-types spec §3.4 caps the Blaze stage at 3 pooled shadowless point
	 * lights, and T-09 confirms three fields on Tutorial_Island - so the coupling was quietly
	 * putting up to 30 dynamic lights on a map whose lighting is meant to stay static at dusk.
	 *
	 * 3 is the spec number and it holds up visually because the lights overlap so heavily: at
	 * FireVolumeRadius 170 and AGSFireVolume::LightRadiusScale 6 each light reaches ~1000uu, which
	 * is nearly four cells at CellSize 270. Three of those cover a burning band that is typically
	 * about four cells deep, so the ground under the front is lit either way; what the other seven
	 * lights were adding was overlap, not coverage.
	 *
	 * The lit slots are the LOW slot indices. Slot assignment is sticky (see VolumeCellIndices), so
	 * the lit slots are stable actors rather than a set that reshuffles every update - the light
	 * pool crawls with the front exactly as the flames do, instead of flickering between volumes.
	 *
	 * 0 is legal and means "flames only, no dynamic light from the field at all".
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage", meta = (ClampMin = "0"))
	int32 MaxLitVolumes = 3;

	/** Smaller now that there are more of them - patches, not slabs. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage")
	float FireVolumeRadius = 170.f;

	// IntensitySaturationCells was removed on 2026-07-31. It fed a SECOND, incompatible definition
	// of "how involved is this fire" - local burning DENSITY around the volume - alongside the burn
	// arc the cells themselves run on. The two peaked at different moments, which is why the flames
	// never lined up with where the spread was actually pushing. UpdateDamageVolumes now feeds
	// SetFireIntensity straight from GetCellIntensity01 of the cell the volume is parked on, so the
	// flame's arc, the heat the cell radiates and the mask's ember channel are literally the same
	// number. The small-flame floor the density term used to provide is already covered by
	// AGSFireVolume::MinIntensityScale.

	/**
	 * Line-trace each volume down onto the ground instead of leaving it on the grid's flat plane.
	 *
	 * The cell grid is a single plane, which is fine on the hamlet generator's flat farm plot but
	 * wrong anywhere real: on Tutorial_Island the ground under one field falls ~1300uu from the
	 * windmill to the field edge, so half the fire would hang in the air and half would be buried.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage")
	bool bSnapVolumesToGround = true;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Damage")
	float GroundSnapTraceHeight = 2500.f;

	/**
	 * The field owns its own smoke, so the pooled volumes are told not to draw any (2026-07-31).
	 *
	 * Objective 3 is "smoke must stay behind after the fire moves on", and a plume parented to a
	 * pooled AGSFireVolume is architecturally incapable of that: the volumes are parked on BURNING
	 * cells and repositioned as the front advances, so the smoke leaves with the fire by
	 * definition. The fix is SmokeWisps below - a separate pool that sits on BURNT cells and stays
	 * there. Once that is running, leaving smoke on the volumes as well double-counts the plume in
	 * exactly one place: on the active front, which is the one part of the field that already
	 * reads clearly.
	 *
	 * Set false to revert to the pre-2026-07-31 behaviour (volumes smoke, no wisps behind).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Field|Damage")
	bool bFieldOwnsSmoke = true;

	// ------------------------------------------------------------------ burn mask (objective 1)
	//
	// THE MASK ITSELF LIVES IN UGSBurnMaskSubsystem SINCE 2026-07-31. Everything that used to be
	// configured here - BurnMaskResolution, BurnSplatRadiusCells, BurnMaskBrush, the redraw
	// interval, bAutoBindCropMaterials, CropActorTag - is now world-scoped config on the subsystem
	// (Config = Game, so DefaultGame.ini rather than per-actor defaults). A field has no business
	// owning a render target once the mask covers the level, and three fields each owning one is
	// precisely the architecture that could not mark painted foliage at all.
	//
	// What the field still owns is WHAT TO SPLAT AND WHEN: HandleCellChangedForFX splats on state
	// change, CosmeticTick re-splats live cells so the transient channels survive the subsystem's
	// per-flush decay. The channel contract below is unchanged and is reproduced here because it
	// is the field that fills the channels.
	//
	/**
	 * CHANNEL CONTRACT - crop materials depend on this. Do not repurpose channels:
	 *     R = burnt-ness.  0 unburnt -> 1 fully charred. MONOTONIC, never decreases: it is driven
	 *                      by FGSFieldCell::CharHigh, a high-water mark that is only ever raised
	 *                      and is pinned to 1 once the cell is Burnt, so a doused cell keeps the
	 *                      char it earned. Blend charred albedo and flatten the crop normal with
	 *                      this.
	 *                      (Corrected 2026-07-31: this used to say R was driven by BurnSeconds,
	 *                      "which is never reset". That was only true on the server - on a client
	 *                      OnRep_CellStates zeroes BurnSeconds on every re-ignition to restart the
	 *                      G arc, so a doused-then-re-torched cell visibly un-charred. The two
	 *                      values are now separate for exactly that reason.)
	 *     G = burning now. The cell's GetCellIntensity01 arc - rises and falls with the flame and
	 *                      is 0 everywhere else. Drive emissive ember glow with this.
	 *     B = doused/wet.  1 while the brigade's water is on the crop. Darken and gloss the wheat.
	 *                      The defenders' one verb should be visible ON the field, not only as an
	 *                      absence of fire.
	 *
	 * This layer is COSMETIC and therefore runs on clients as well as the server. It is fed by
	 * OnFieldCellChanged, which fires on both (on clients via OnRep_CellStates), and deliberately
	 * NOT by CellTick, which is authority-only - drive it from there and a co-op client sees an
	 * untouched golden field with fire standing on it.
	 *
	 * HOW THE THREE CHANNELS ARE ACTUALLY FILLED, now that the mask is an additive accumulator
	 * rather than a full redraw (2026-07-31):
	 *     R - deposited in slices. CosmeticTick deposits Step / CellBurnSeconds per tick while a
	 *         cell burns, so a full burn integrates to exactly 1.0, and HandleCellChangedForFX
	 *         tops up the REMAINDER when a cell reaches Burnt so ash blackens the ground at once
	 *         rather than several ticks later. Deposits only ever add, so monotonic holds by
	 *         construction - there is no code path that can lower a texel.
	 *
	 *         (Corrected 2026-07-31: the transition path used to deposit CharHigh outright on
	 *         EVERY state change, on top of the slices, so a doused-and-re-torched cell paid its
	 *         char three or four times and R saturated in the first seconds of the burn. Both
	 *         paths now spend from one per-cell budget - FGSFieldCell::CharDeposited.)
	 *     G - re-splatted every CosmeticTick from GetCellIntensity01, because the subsystem fades
	 *         it between flushes.
	 *     B - re-splatted every CosmeticTick while a cell is Doused, same reason.
	 *
	 * R AND G ARE SCALED BY UGSBurnMaskSubsystem::GetOverlapCompensation(CellSize) on the way out
	 * (2026-07-31). Several cells deposit into the same texel every tick, for two compounding
	 * reasons - the splat quad is wider than a cell, and a texel is itself wider than a cell at
	 * the mask's resolution - so uncompensated, one cell's worth of burning landed as several
	 * cells' worth of char and ember and R saturated in a fraction of the burn. CellSize is passed
	 * in because the cell pitch is the field's and the rest of the arithmetic is the mask's.
	 * B is not scaled - it is a flag, not an accumulator.
	 */
	/** Cosmetic cadence: how often the client-side burn clock advances, the smoke wake updates,
	 *  and live cells re-splat the mask's transient channels.
	 *
	 *  Renamed from BurnMaskRedrawInterval on 2026-07-31 (the field no longer redraws anything).
	 *  A rename drops any value a Blueprint subclass had saved under the old name, which is
	 *  acceptable here because nothing in Content overrides it and the default is unchanged.
	 *  Kept slightly SLOWER than the subsystem's 0.15 s flush on purpose: the decay is exponential
	 *  rather than a hard clear, so a re-splat that lands every second or third flush still holds
	 *  a steady glow. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Field|BurnMask", meta = (ClampMin = "0.05"))
	float CosmeticTickInterval = 0.2f;

	// ------------------------------------------------------------------ smoke wake (objective 3)

	/**
	 * Smoke that STAYS BEHIND, which is the whole of objective 3 (2026-07-31).
	 *
	 * Until today the only smoke in a field fire came from AGSFireVolume::SmokeFX, and those
	 * volumes live on the BURNING cells and are repositioned as the front advances. The smoke
	 * therefore walked off with the fire and the razed ground behind it was clean - the exact
	 * opposite of what a burnt field looks like, and unfixable by tuning because it is a
	 * consequence of what the plume is attached to.
	 *
	 * These wisps are a separate pool with a different job: they are assigned to BURNT cells and
	 * they stay there. Burnt is terminal, so in practice a wisp is placed once and smoulders until
	 * the raid ends. That persistence is the point - the GDD wants a razed objective to read from
	 * the treeline, and at that distance a smoke column is the only thing that carries.
	 *
	 * Deliberately plain UNiagaraComponents attached to this actor: no damage, no light, no
	 * AGSFireVolume. A burnt field should not keep cooking anything that walks over it, and a
	 * dozen extra unshadowed point lights over ground with no flame on it would be both wrong and
	 * expensive.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Smoke", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxSmokeWisps = 12;

	/**
	 * The smoulder system. Soft pointer to an asset that DOES NOT EXIST YET - which is exactly why
	 * it is soft. Everything here must run correctly today with this unresolved: the wisp
	 * components are still created and placed, they simply have no asset on them, and the warning
	 * fires once rather than per wisp per update.
	 *
	 * Wants a slow, wide, low-opacity drifting column - closer to haze than to a chimney. It is
	 * standing over ash, not over flame.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Field|Smoke")
	TSoftObjectPtr<UNiagaraSystem> SmokeWispSystem;

	/**
	 * How long a wisp holds its cell before it may be re-seeded onto more recently burnt ground.
	 * 0 = never re-seed, i.e. the pre-2026-07-31 behaviour.
	 *
	 * THE WAKE HAS TO MIGRATE WITH THE FRONT. The first version of this pool combined sticky
	 * assignment with a state (Burnt) that is terminal, and the two together mean the first
	 * MaxSmokeWisps cells to finish burning hold all the wisps for the rest of the raid. On the
	 * default 5x8 field that is 12 wisps pinned to the 12 cells nearest the torch, and the other
	 * 28 razed cells never smoke at all - so the smoke column marks where the fire STARTED rather
	 * than trailing the fire as it moves. Objective 3 is "smoke stays behind after the fire moves
	 * on", and a wake that cannot move is only half of that.
	 *
	 * Re-seeding takes the MOST RECENTLY BURNT unclaimed cell, not the nearest one, precisely
	 * because the point is to follow the front. Gated on age so a wisp still stands still for a
	 * good long while first - a wisp that chased the front every update would be a second set of
	 * fire volumes, not a wake. 45 s is about two cell burns: long enough that any given column
	 * reads as permanent to a player watching it, short enough that the pool has cycled across the
	 * field by the time a 5x8 grid is fully razed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Field|Smoke", meta = (ClampMin = "0.0"))
	float WispReseedSeconds = 45.f;

	// ------------------------------------------------------------------ the jump (decision 26)

	/** Burnt edge cells reach out and light fences, haycarts, a granary built too close. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Jump")
	bool bCanJumpToAdjacentFlammables = true;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Jump")
	float JumpRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Field|Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpChance = 0.35f;

	// ------------------------------------------------------------------ state

	/** Server-authoritative rich state. Not replicated directly - BurnSeconds is server business. */
	UPROPERTY()
	TArray<FGSFieldCell> Cells;

	/**
	 * The replicated packed state array (spec §3.1: "one replicated packed state array + RepNotify").
	 * One byte per cell of EGSFieldCellState. A 5x8 field is 40 bytes; even a 16x16 is 256.
	 *
	 * This replaced a per-cell unreliable NetMulticast. The multicast was wrong in a way that only
	 * shows up in co-op: a dropped packet left a client's cell permanently mis-rendered with no
	 * mechanism to correct it, and a client had no authoritative grid to build the Niagara position
	 * array from in the first place.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CellStates)
	TArray<uint8> ReplicatedCellStates;

	/** Live pooled damage volumes, at most MaxFireVolumes of them. */
	UPROPERTY()
	TArray<TObjectPtr<AGSFireVolume>> DamageVolumes;

	/**
	 * Which cell each pooled volume is currently parked on. Parallel to DamageVolumes; INDEX_NONE
	 * means unassigned.
	 *
	 * Added 2026-07-31 after Michael: "there's no consistency to how the fire is spreading."
	 *
	 * The original assignment sampled evenly-spaced indices into the burning-cell array every
	 * update. That array's CONTENTS and ORDER change every tick as cells catch and burn out, so
	 * the same slot resolved to a different cell constantly and each volume TELEPORTED around the
	 * field - flames winking out here and reappearing forty metres away, with no relationship to
	 * where the fire had actually spread. The cell simulation underneath was always coherent; the
	 * thing being rendered was ten lights being shuffled through a list.
	 *
	 * Sticky assignment fixes the read: a volume holds its cell until that cell stops burning,
	 * then moves to the NEAREST unclaimed burning cell instead of wherever its index lands. Fire
	 * now crawls from where it was to somewhere adjacent, which is what spreading looks like.
	 */
	TArray<int32> VolumeCellIndices;

	/**
	 * The smoke wake (2026-07-31). Parallel to WispCellIndices and WispAges, capped at
	 * MaxSmokeWisps.
	 *
	 * Same sticky-assignment discipline as the damage volumes and for the same reason: a wisp that
	 * re-derived its cell from a re-sorted array every update would teleport, and a teleporting
	 * smoke column reads far worse than a teleporting flame because it is the thing meant to
	 * signal permanence.
	 *
	 * Sticky is not PERMANENT, though, and the first version of this made that mistake: Burnt is
	 * terminal, so nothing ever released a wisp and the pool stayed on the first cells to finish.
	 * WispReseedSeconds is the release valve - see it for why the wake has to migrate.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> SmokeWisps;

	/** Which BURNT cell each wisp stands on. Parallel to SmokeWisps; INDEX_NONE = unassigned. */
	TArray<int32> WispCellIndices;

	/** Seconds each wisp has stood on its current cell. Parallel to SmokeWisps; reset to 0 on
	 *  every (re-)seed. Read only against WispReseedSeconds. */
	TArray<float> WispAges;

	// BurnMaskRT, CropMaterials, bBurnMaskDirty, bWarnedMissingBurnBrush, ResolvedBurnMaskBrush and
	// bBurnMaskBrushResolveFailed were all REMOVED on 2026-07-31. The render target, the MID cache
	// and the brush now belong to UGSBurnMaskSubsystem, which owns one of each for the entire
	// world. The dirty flag went with them: there is nothing local to redraw, and the coalescing it
	// provided is done better by the subsystem's splat queue, which batches across every source in
	// the level rather than only across this field's cells.

	/** One-shot warning latch. A missing soft asset should say so once, not once per update. */
	bool bWarnedMissingSmokeSystem = false;

	/**
	 * RESOLVE-ONCE CACHE for the smoke system (2026-07-31).
	 *
	 * The warning latches above suppress our own log line, and until today that was mistaken for
	 * suppressing the WORK. It is not: LoadSynchronous() was still being called every update -
	 * four times a second for the wisps, five times a second for the mask - and LoadSynchronous
	 * does not cache a FAILURE. SmokeWispSystem's default points at an asset that does not exist
	 * yet, so every one of those calls was a full failed package lookup plus an engine-side
	 * warning, for the entire length of the raid, on every machine that renders.
	 *
	 * So resolution happens once and the answer is kept - including the answer "there is nothing
	 * there", which is what the paired bool records. Once the failure latch is set the load is
	 * skipped entirely rather than retried; a soft asset that is missing at BeginPlay is not going
	 * to appear mid-raid, and if one is ever authored the fix is to restart PIE, not to pay for a
	 * failed lookup 4 Hz forever.
	 *
	 * Transient because these are resolved pointers to content, not saved state.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ResolvedSmokeSystem;

	bool bSmokeSystemResolveFailed = false;

	int32 BurningCellCount = 0;
	int32 BurntCellCount = 0;
	int32 DousedCellCount = 0;

	/** Ever-increasing stamp source for FGSFieldCell::BurntSequence. Advanced on both the server
	 *  and clients, each from its own cell transitions, so the smoke wake can order razed ground
	 *  by recency without replicating a single extra byte. */
	int32 BurntSequenceCounter = 0;

	FTimerHandle CellTickHandle;

	/** Cosmetic timer (renamed from BurnMaskTimerHandle, 2026-07-31). Separate from CellTickHandle
	 *  on purpose: CellTickHandle is started only on the authority, this one runs everywhere that
	 *  has a renderer. */
	FTimerHandle CosmeticTimerHandle;

	/** Cells are not UGSFlammableComponents, so nothing arms the unseen-fire fuse on our behalf -
	 *  we have to report the first cell ourselves or a field burning alone never promotes the
	 *  town to Raid (design doc §8.1, decision 19). */
	bool bHasReportedFireStarted = false;
};
