// The player plants the Warren (GDD §6, ruling of 2026-08-21).
//
// THIS REVERSES RULING 36, recorded earlier the same day, which said "placed-not-planted" and "there
// is no planting channel and no digger". There is a planting channel now; the player is the digger.
// The reversal is deliberate and Michael's - see AgentQueue ticket #245 - and is recorded rather
// than quietly rewritten so the record shows the design went both ways.
//
// HOLD T, not X. X is IA_GuardBreak and always was; the earlier note that it looked free came from
// grepping Source/ for EKeys::X, which finds nothing because bindings live in the input asset.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSWarrenPlacementComponent.generated.h"

class AGSWarren;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Why the ghost is red. Reported rather than merely shown, because "I cannot place here" and "I do
 *  not know why I cannot place here" are different player experiences. */
UENUM(BlueprintType)
enum class EGSWarrenPlacementBlock : uint8
{
	None            UMETA(DisplayName = "Placeable"),
	NoGround        UMETA(DisplayName = "Nothing to plant it in"),
	GroundTooSteep  UMETA(DisplayName = "Ground too steep"),
	Obstructed      UMETA(DisplayName = "Mouth is blocked"),
	OnCooldown      UMETA(DisplayName = "Still digging out the last one")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSOnWarrenPlacementChanged, bool, bPlacing, EGSWarrenPlacementBlock, Block);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWarrenPlanted, AGSWarren*, Warren);

/**
 * Hold T to raise a ghost of the Warren gate, release to plant it.
 *
 * ONE WARREN PER PLAYER, MANY IN THE WORLD. Michael, 2026-08-21: "multiple warrens, each player can
 * only place 1 but we're building for multiplayer in the future." That per-player limit is why the
 * planted Warren is tracked HERE rather than as an owner field on AGSWarren: every player owns one
 * component, so the component holding at most one Warren enforces the rule by construction, with no
 * ownership plumbing and no change to AGSWarren at all. The nearest-Warren lookups deliberately stay
 * owner-blind - "loot goes to the closest warren portal" does not care who dug it.
 *
 * Planting a second one COLLAPSES the first where it stands. The alternative - refusing until the
 * cooldown expires - leaves a player stuck with a badly-placed Warren for three minutes, which is a
 * worse failure than losing a good one.
 */
UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSWarrenPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSWarrenPlacementComponent();

	/** T went down. Raises the ghost. Safe to call while already placing - it no-ops rather than
	 *  restarting, so an input system that repeats Started cannot flicker the preview. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Warren")
	void BeginPlacement();

	/** T came up. Plants if the ghost is green, otherwise just lowers it. Deliberately does NOT
	 *  refuse loudly on a red ghost: the player has been looking at a red gate the whole time and
	 *  telling them again on release is nagging. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Warren")
	AGSWarren* ConfirmPlacement();

	/** Lower the ghost without planting. For death, weapon swap, anything that should interrupt. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Warren")
	void CancelPlacement();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	bool IsPlacing() const { return bPlacing; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	EGSWarrenPlacementBlock GetBlockReason() const { return BlockReason; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	bool IsPlacementValid() const { return BlockReason == EGSWarrenPlacementBlock::None; }

	/** Seconds until another Warren may be planted. 0 means now. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	float GetCooldownRemaining() const;

	/** This player's Warren, or null if they have not planted one (or it has been destroyed). */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Warren")
	AGSWarren* GetPlantedWarren() const { return PlantedWarren.Get(); }

	/** Fires when the ghost goes up or down, and whenever the block reason changes while it is up.
	 *  A HUD prompt binds here rather than polling. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Warren")
	FGSOnWarrenPlacementChanged OnPlacementChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Warren")
	FGSOnWarrenPlanted OnWarrenPlanted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** What gets planted. Soft, and loudly complained about if unset - a placement channel with no
	 *  Warren class is a key that does nothing, which is the hardest kind of bug to notice. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren")
	TSoftClassPtr<AGSWarren> WarrenClass;

	/** The ghost's mesh. The Warren's own visual is a Niagara rune rather than a mesh, so the
	 *  preview needs its own stand-in for the gate. Unset means no visible ghost - the placement
	 *  still works and still reports valid/invalid, but the player cannot see where it will land,
	 *  so BeginPlay says so once. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Ghost")
	TSoftObjectPtr<UStaticMesh> GhostMesh;

	/** Translucent green. Michael: "a transparent green material over the gate to indicate you can
	 *  place it, or a transparent red to indicate there's something blocking it." */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Ghost")
	TSoftObjectPtr<UMaterialInterface> GhostMaterialValid;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Ghost")
	TSoftObjectPtr<UMaterialInterface> GhostMaterialInvalid;

	/**
	 * Lifts the GHOST off the ground - the visual only, never the spawn point.
	 *
	 * Without it the ghost's surface is coplanar with the floor and the depth test flips between
	 * them frame to frame, which reads as the colour flickering rather than as z-fighting. The
	 * Warren still plants exactly on the traced ground: this offset is applied where the ghost is
	 * drawn and nowhere else.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Ghost", meta = (ClampMin = "0.0"))
	float GhostVisualLift = 8.f;

	/** How far in front of the player the gate lands.
	 *
	 *  A FIXED DISTANCE, not the player's aim. Predictable beats flexible here: you learn "it goes
	 *  there" in one use, and it cannot be smeared across the map by a camera that happens to be
	 *  pitched at the sky. Revisit if placing it precisely turns out to matter. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "50.0"))
	float PlacementDistance = 250.f;

	/** How far up and down of the player's feet to look for ground at that spot. Generous downward
	 *  so the gate can be planted off a low ledge; shallow upward so it cannot climb a wall. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "50.0"))
	float GroundTraceUp = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "50.0"))
	float GroundTraceDown = 600.f;

	/**
	 * Steeper than this and the ghost goes red.
	 *
	 * NOT the 65 degrees that goblins can walk and climb. A goblin can scramble up a slope it cannot
	 * plant a gate in; the mouth has to hold still and things have to be able to climb out of it.
	 * 35 is a guess and is EditDefaultsOnly so it can be tuned against a real hillside.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxGroundSlopeDegrees = 35.f;

	/** Radius of the clearance check at the spot. Wants to be about the mouth's own footprint - big
	 *  enough that goblins are not climbing out inside a wall, small enough that a doorway is still
	 *  a legal place to put one. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "10.0"))
	float ClearanceRadius = 120.f;

	/** How far ABOVE ClearanceRadius the clearance sphere sits. Must be > 0 or the sphere rests on
	 *  the ground and the floor itself reads as an obstruction - which is exactly what happened on
	 *  the flat arena floor, where every spot came back red. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "1.0"))
	float ClearanceGroundLift = 10.f;

	/** Michael's ruling, 2026-08-21: "let's give it a 3 minute cooldown also for summoning a new
	 *  one." Counted from the moment one is planted, not from the moment the previous is lost. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Warren|Placement", meta = (ClampMin = "0.0"))
	float PlacementCooldownSeconds = 180.f;

private:
	/** Where the gate would land right now, and why it may not. Sets BlockReason as a side effect
	 *  and broadcasts if it changed. */
	bool EvaluateSpot(FVector& OutLocation, FRotator& OutRotation);

	/** Builds the ghost on first use and keeps it. A player who never plants never pays for it. */
	void EnsureGhost();
	void ShowGhost(bool bVisible);
	void ApplyGhostMaterial(bool bValid);

	void SetBlockReason(EGSWarrenPlacementBlock NewReason);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GhostComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMIDValid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMIDInvalid;

	/** Weak on purpose: the Warren can be destroyed by anything - a level unload, a debug command,
	 *  a future defender who collapses it - and this must read as "no Warren" the moment it is. */
	TWeakObjectPtr<AGSWarren> PlantedWarren;

	bool bPlacing = false;
	bool bGhostMaterialIsValid = true;
	EGSWarrenPlacementBlock BlockReason = EGSWarrenPlacementBlock::None;

	/** World seconds at which the last Warren was planted. Negative means never. */
	double LastPlantedTime = -1.0;
};
