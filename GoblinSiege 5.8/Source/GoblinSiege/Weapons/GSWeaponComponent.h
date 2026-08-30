// Weapon identity lives here - equip/swap is a data operation (design doc §5). Owns the
// Scout's melee/ranged toggle (with the anti-cancel input lock) and the Blood Staff orb
// economy. Reconstructed 2026-07-19 to match the surviving GSPlayerCharacter.cpp caller.
// ("Slasher" corrected to "Scout" on 2026-08-01 - design doc §13 decision 36 as amended that day.
// Only comments touched by this pass were corrected; other files still say Slasher.)
//
// 2026-08-01 - THIS COMPONENT NOW SPAWNS AND MOVES THE WEAPON MESHES. Before today it equipped a
// weapon's stats, applied its turn rate and granted its abilities, and that was the whole of
// "equip": nothing was ever created, attached, or drawn, so a fully-equipped Scout stood in the
// hamlet with empty hands. The mesh half of the data asset (UGSWeaponDataAsset's
// GoblinSiege|Weapon|Visual block) is consumed here.
//
// Up to four UStaticMeshComponents are created at runtime and REUSED across equips rather than
// destroyed and recreated: melee, ranged, quiver, held torch. Runtime NewObject + RegisterComponent
// rather than CreateDefaultSubobject, because which meshes exist at all is a property of the data
// asset that is equipped, which is not known at construction time and changes on every swap.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSWeaponComponent.generated.h"

class UGSWeaponDataAsset;
class UStaticMesh;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;

// EGSWeaponSlot LIVED HERE until #274. It is now four gameplay tags - GSTags::WeaponSlot_Torch /
// _Bow / _Sword / _Grapple, declared in Combat/GSGameplayTags.h.
//
// The enum carried an APPEND ONLY warning because its integer values were serialised into every
// adversary CDO, so inserting a slot silently repointed them. Tags serialise by NAME, so that whole
// class of hazard is gone and the wheel's contents became data (see WheelSlots below).
//
// A WeaponSlot is NOT an ItemSlot. The ItemSlot.* tags added in #271 are where an item physically
// hangs (hand_r_weapon, back_sword); a WeaponSlot is which loadout the player has chosen. A torch
// and a sword both hang in ItemSlot.RightHand and are different WeaponSlots.

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnOrbCountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWeaponModeChanged, bool, bRangedMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWeaponSlotChanged, FGameplayTag, NewSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWheelOpenChanged, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnWheelHighlightChanged, FGameplayTag, Highlighted);

UCLASS(ClassGroup = (GoblinSiege), meta = (BlueprintSpawnableComponent))
class GOBLINSIEGE_API UGSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSWeaponComponent();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon")
	void EquipWeapon(UGSWeaponDataAsset* NewWeapon);

	/** Rebuild every weapon mesh from the current data asset. CallInEditor so socket offsets and
	 *  scales can be tuned from the details panel instead of via a PIE restart per attempt
	 *  (2026-08-02 - the offset-tuning loop this unblocks is worth an afternoon). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GoblinSiege|Weapon")
	void RefreshWeaponVisuals();

	/**
	 * Suppress the HOLSTERED melee weapon while the player is aiming.
	 *
	 * `back_sword` sits on Spine02 at (12, 6, 30) and the axe wears a 1.6 scale, so a large mesh is
	 * parked high on the back - which is exactly where an over-the-shoulder aim camera is pointing.
	 * Nothing is misconfigured; the holster and the aim camera simply want the same space.
	 *
	 * Only the holstered half is affected. A weapon actually IN HAND is never hidden by this, so
	 * aiming the torch still shows the torch.
	 */
	void SetAimActive(bool bInAimActive);

	/** Scout sword⇄bow toggle. Brief input lock so the swap can't be used as a frame-perfect
	 *  combat cancel (race-design-goblins.md weapon-swap rule).
	 *
	 *  2026-08-01: this now MOVES MESHES as well as flipping the mode flag - the newly-active half
	 *  goes to its hand socket and the newly-inactive half to its holster socket (or hides, if the
	 *  weapon sets bShowHolsteredWeapon false). The quiver is untouched by the swap; it is worn
	 *  continuously. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon")
	void ToggleRangedMode();

	/** True only for the BOW. The torch is thrown, but it is not "ranged mode" - that flag gates the
	 *  bow ability, the aim camera and the heavy-charge suppression, none of which the torch wants.
	 *  Kept rather than replaced because a dozen callers and any Blueprint asking "am I holding the
	 *  bow" still want exactly this question. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	bool IsInRangedMode() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	FGameplayTag GetCurrentSlot() const { return CurrentSlot; }

	/**
	 * Put a specific slot in hand. The single choke point every path goes through - the wheel, the
	 * legacy sword/bow toggle, and anything a Blueprint does - so mesh placement, the torch prop and
	 * the three broadcasts can never disagree about what is being held.
	 *
	 * Refuses the same three ways ToggleRangedMode does (swap lock, no weapon, no ranged half), each
	 * naming its own cause, and refuses the Bow slot on a weapon with bHasRangedMode unticked.
	 * Returns false if the slot did not change, for any reason.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon")
	bool SetSlot(FGameplayTag NewSlot);

	// --- Radial wheel (2026-08-06) ---------------------------------------------------------------
	//
	// Michael's design: hold Q, drag a direction, release to commit; drag back to centre to cancel.
	// The maths is here rather than in the widget because the selection must be identical whether or
	// not a widget is on screen - the UMG layer is unbuilt, and this works without it.
	//
	// Accumulated mouse DELTA, deliberately, not cursor position: the game has no visible cursor and
	// a position-based wheel would need one warped to centre on open, which fights the camera.

	/** Begin a wheel selection. Zeroes the accumulator, so the drag is measured from wherever the
	 *  mouse happens to be. Highlight starts at the CURRENT slot, so releasing without moving is a
	 *  no-op rather than a random pick. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|Wheel")
	void OpenWeaponWheel();

	/** Feed a frame's mouse delta in while the wheel is open. Screen convention: +X right, +Y DOWN. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|Wheel")
	void AddWheelInput(FVector2D Delta);

	/** Release. bCommit false (or a drag still inside the dead zone) closes without changing slot. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|Wheel")
	void CloseWeaponWheel(bool bCommit = true);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Wheel")
	bool IsWheelOpen() const { return bWheelOpen; }

	/** The slot that would be committed right now. Equals the current slot while inside the dead
	 *  zone, which is what makes "drag back to centre" read as a cancel. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Wheel")
	FGameplayTag GetWheelHighlight() const { return WheelHighlight; }

	/** Accumulated drag, for a widget that wants to draw the stick. Not normalised. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Wheel")
	FVector2D GetWheelVector() const { return WheelAccum; }

	/** True once the drag has left the dead zone - i.e. releasing now would actually change slot. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Wheel")
	bool IsWheelCommitted() const { return WheelAccum.Size() >= WheelDeadZone; }

	/**
	 * Direction -> slot, exposed so the widget draws the sectors the input actually uses instead of
	 * a second copy that can drift. Screen convention (+Y down). Inside the dead zone this returns
	 * FallbackSlot unchanged.
	 *
	 * Top is Torch, right is Bow, bottom is Grapple, left is Sword - 90-degree sectors centred on
	 * each. Was three 120-degree sectors until the grapple arrived (2026-08-17); Bow and Sword each
	 * moved 30 degrees to make room, which keeps them on the side of the wheel a player already
	 * reaches for rather than reshuffling the whole thing.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Wheel")
	FGameplayTag SlotForDirection(FVector2D Direction, FGameplayTag FallbackSlot) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UGSWeaponDataAsset* GetEquippedWeapon() const { return EquippedWeapon; }

	// --- Visible weapons (2026-08-01) ---

	/**
	 * The mesh component for whichever half of the sword⇄bow pair is currently IN HAND, or null if
	 * nothing is equipped / the mesh is unset / the mesh failed to resolve.
	 *
	 * Exists for the attack abilities that do not exist yet: a sword swing wants to trace along the
	 * blade's own bounds, and asking the weapon component "what am I actually holding" is a great
	 * deal more honest than an ability hardcoding a socket name and a length. Deliberately does NOT
	 * return the held torch even while the torch is readied - the torch is a separate tool slot
	 * with its own ability (tech doc §16's GA_TorchBonk), and a sword ability that silently traced
	 * along a torch would be a bug nobody would find. Use GetHeldTorchMesh() for that.
	 *
	 * Null is a normal, expected return. Every caller must handle it: the whole visual layer
	 * degrades to "invisible but functional" when the art isn't in yet.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon")
	UStaticMeshComponent* GetActiveWeaponMesh() const;

	/** The readied-torch prop, or null. Non-null does not mean visible - check IsTorchReadied(). */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Torch")
	UStaticMeshComponent* GetHeldTorchMesh() const { return HeldTorchMeshComponent; }

	/** The readied-torch's flame, or null - unset HeldTorchFlameSystem is a legal, silent state. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Torch")
	UNiagaraComponent* GetHeldTorchFlameFX() const { return HeldTorchFlameFXComponent; }

	/**
	 * Show/hide the torch in the goblin's off hand. Called by UGSGA_TorchToss around its throw
	 * (readied on activation, gone the instant the projectile leaves), and intended as the hook the
	 * future IA_EquipTorch toggle (tech doc §16, Q) drives directly once the torch becomes a
	 * persistent tool slot rather than a one-shot ability.
	 *
	 * Creates the mesh component lazily on first ready rather than at equip: a goblin who never
	 * throws a torch never pays for one, and the torch mesh is per-weapon-asset data that may
	 * legitimately be unset.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|Torch")
	void SetTorchReadied(bool bNewReadied);

	/**
	 * Tell ACF what is now in hand, if the migration flag is on and this slot maps to an item slot.
	 *
	 * Called from SetSlot and ONLY from SetSlot. UseEquippedItemBySlot must never be reachable from
	 * input directly: it has no anti-cancel lock of its own, and re-selecting the slot you already
	 * hold makes it SHEATHE everything rather than no-op. SetSlot's own same-slot early return is
	 * what stops that second case ever reaching ACF.
	 */
	void SyncACFEquippedSlot();

	/**
	 * True when ACF is holding the weapon for this slot, so our own mesh path must stay out of it.
	 *
	 * Requires all three: the migration flag, a mapping for the slot, AND an item actually equipped
	 * in that item slot. The third is what makes the migration stageable - the bow is mapped from
	 * stage 1 but nothing is equipped in the off hand until stage 3, so ACF does not own it yet and
	 * our mesh path still does.
	 */
	bool IsSlotOwnedByACF(FGameplayTag Slot) const;

	/**
	 * Push the holstered-melee visibility rule onto ACF's weapon actor.
	 *
	 * ACF has NO equivalent of bShowHolsteredWeapon - its only way to express "invisible while
	 * holstered" is clearing the on-body socket, which hides the weapon always. So the rule stays
	 * ours and is applied to ACF's actor from here.
	 *
	 * MUST be re-applied after OnEquipmentChanged. RefreshEquipment calls AttachWeaponOnBody for
	 * every non-drawn weapon on every equipment change, and that SHOWS the actor - so a hide applied
	 * once is undone by the next replication tick, and the axe pops back mid-bow.
	 */
	void RefreshACFWeaponVisibility();

	/**
	 * Push the per-weapon mesh offset from the WEAPON DATA ASSET onto ACF's weapon actor.
	 *
	 * THE DATA ASSET IS THE SINGLE SOURCE OF TRUTH FOR WEAPON PLACEMENT, and this is what makes that
	 * true for the ACF path as well as ours.
	 *
	 * Until #293 the offset existed TWICE: once in UGSWeaponDataAsset::MeleeMeshOffset/RangedMeshOffset,
	 * and again as a hand-copied duplicate on the static mesh component inside each BP_ACFWeapon_*.
	 * The plan for the migration called that transcription its biggest concrete cost and it was right -
	 * editing the data asset moved our mesh and did NOTHING to the weapon ACF was holding, so the field
	 * that looks authoritative in the editor silently was not. Nobody could tune a weapon by eye.
	 *
	 * Applied to the static mesh component rather than through ACF's AttachmentOffset because
	 * AACFWeaponActor::Mesh is a SKELETAL mesh component and every weapon mesh in this project is
	 * static - so the mesh lives on a component added in the Blueprint, which AlignWeapon never touches.
	 */
	void ApplyACFWeaponOffsets();

	/** Which data-asset offset drives a given wheel slot: Primary is the melee half, Bow the ranged
	 *  half. Null for a slot the data asset has no offset for. */
	const FTransform* OffsetForWeaponSlot(FGameplayTag Slot) const;

	/**
	 * GS.Weapon.Set - shove one offset onto whatever weapon this character is holding, right now.
	 *
	 * Deliberately writes to the live COMPONENT and never to an asset: a value typed at a console
	 * during a look-and-tweak pass should not be able to save itself into the project. The value that
	 * survives is the one somebody types into the data asset, which is also the one Reapply reads.
	 *
	 * Covers both paths - ACF's weapon actor and our own mesh components - because which one is
	 * holding the weapon depends on a per-character flag, and somebody tuning by eye should not have
	 * to know which. Returns how many meshes it actually moved, so a command that matched a character
	 * but moved nothing reports 0 rather than looking like it worked.
	 */
	int32 DebugApplyOffsetToHeldWeapon(const FTransform& Offset);

	/** GS.Weapon.Reapply - re-read the weapon data asset and push its offsets onto a weapon already
	 *  in hand, so an edit made in the editor can be seen without restarting PIE. */
	void DebugReapplyOffsets();

	/** Re-entrancy guard. UseEquippedItemBySlot broadcasts OnEquipmentChanged, which we listen to -
	 *  so drawing from inside that handler would call straight back into itself. */
	bool bSyncingACF = false;

	UFUNCTION()
	void HandleACFEquipmentChanged(const FEquipment& NewEquipment);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|Torch")
	bool IsTorchReadied() const { return bTorchReadied; }

	/**
	 * Raise or lower the war-horn prop. Driven by UGSGA_Horn for the length of a blast.
	 *
	 * NOT a weapon slot, and deliberately not routed through SetSlot: the horn is universal kit that
	 * every class carries, it is never "equipped", and it does not displace whatever is in hand. It
	 * is a prop that appears while an ability is running, which is exactly what the readied torch
	 * was before the torch became a slot.
	 */
	void SetHornRaised(bool bNewRaised);

	bool IsHornRaised() const { return bHornRaised; }

	// --- Blood Staff orb economy (design doc §5: melee hits bank orbs, Blood Nova spends them) ---

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|BloodOrbs")
	void AddBloodOrb();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Weapon|BloodOrbs")
	bool ConsumeBloodOrbs(int32 Count);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Weapon|BloodOrbs")
	int32 GetBloodOrbCount() const { return BloodOrbs; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon")
	FGSOnOrbCountChanged OnOrbCountChanged;

	/** Kept, and still broadcast on every slot change, so existing C++ and Blueprint bindings survive
	 *  the move to three slots. True only for Bow - see IsInRangedMode. Prefer OnWeaponSlotChanged. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon")
	FGSOnWeaponModeChanged OnWeaponModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon")
	FGSOnWeaponSlotChanged OnWeaponSlotChanged;

	/** Wheel opened / closed. The widget's show and hide. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon|Wheel")
	FGSOnWheelOpenChanged OnWheelOpenChanged;

	/** The highlighted sector changed while dragging. Fires only on CHANGE, not per frame. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Weapon|Wheel")
	FGSOnWheelHighlightChanged OnWheelHighlightChanged;

protected:
	/** See SetAimActive. */
	bool bAimActive = false;

	virtual void BeginPlay() override;

	/** Destroys every mesh component this class created and clears the swap-lock timer. Nothing
	 *  this component builds may outlive it - a stray registered UStaticMeshComponent on a
	 *  destroyed pawn is exactly the kind of leak that only shows up after an hour of respawns. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void GrantAbilitiesFromWeapon();

	// --- Mesh plumbing (2026-08-01) ---

	/** Resolves the equipped weapon's meshes into components (creating or reusing as needed) and
	 *  then places them. Safe to call with no weapon equipped - it tears the meshes down instead. */
	void RebuildWeaponMeshes();

	/** Puts every existing mesh component on the socket its current state calls for: active half in
	 *  hand, inactive half holstered or hidden, quiver where it always is, torch if readied. This
	 *  is the one place attachment state is decided, so equip and swap can't disagree. */
	void RefreshWeaponMeshPlacement();

	/**
	 * Creates (once) or reuses the component in Slot and points it at SoftMesh.
	 *
	 * Reuse rather than churn: a swap that destroyed and recreated components would re-register a
	 * primitive with the scene, rebuild its render state, and re-resolve the same soft path, every
	 * time the player taps the swap key - which is a key the design expects to be tapped mid-fight.
	 * An unset or unresolvable mesh destroys the slot instead, so "the designer cleared the field"
	 * and "the asset went missing" both end at the same honest place: no component, no draw.
	 */
	UStaticMeshComponent* EnsureWeaponMeshComponent(TObjectPtr<UStaticMeshComponent>& Slot,
		const TSoftObjectPtr<UStaticMesh>& SoftMesh, bool& bResolveFailedLatch, const TCHAR* SlotLabel);

	/**
	 * Attaches one mesh to one socket on the owning character's skeletal mesh and applies the
	 * per-weapon offset on top of the snap.
	 *
	 * VALIDATES THE SOCKET FIRST, and this matters more here than it usually would: the goblin
	 * skeleton has NO weapon sockets on it as this is written, so the missing-socket path is not a
	 * defensive nicety, it is the path that runs today. A missing socket falls back to the
	 * character's ROOT component, which parks the weapon at the goblin's feet - visible, obviously
	 * wrong, and impossible to confuse with "the mesh didn't load", which is the failure it would
	 * otherwise be indistinguishable from. Silent is the one thing it must not be.
	 */
	void AttachWeaponMeshToSocket(UStaticMeshComponent* MeshComp, FName SocketName, const FTransform& Offset);

	/** Niagara mirror of EnsureWeaponMeshComponent - same lazy-create/reuse/resolve-failed-latch
	 *  contract, for FX systems rather than meshes (currently just the held torch's flame). */
	UNiagaraComponent* EnsureWeaponFXComponent(TObjectPtr<UNiagaraComponent>& Slot,
		const TSoftObjectPtr<UNiagaraSystem>& SoftSystem, bool& bResolveFailedLatch, const TCHAR* SlotLabel);

	/** Niagara mirror of AttachWeaponMeshToSocket - same socket-validated snap-then-offset contract. */
	void AttachWeaponFXToSocket(UNiagaraComponent* FXComp, FName SocketName, const FTransform& Offset);

	/** Destroys all four mesh components. Called from EndPlay and whenever the weapon is cleared. */
	void DestroyWeaponMeshes();

	/** Starting kit - set on the character Blueprint; EquipWeapon() swaps at runtime. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Weapon")
	TObjectPtr<UGSWeaponDataAsset> EquippedWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Tuning")
	float SwapInputLockSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Tuning")
	int32 MaxBloodOrbs = 5;

	// --- Runtime mesh components (2026-08-01) ---
	// Transient and UPROPERTY: transient because they are rebuilt from the data asset on every
	// equip and must never be serialised into a level or a Blueprint, and UPROPERTY because the GC
	// has to know this component is the thing keeping them alive. A raw pointer here would be a
	// crash waiting for the first garbage collection between an equip and a swap.

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MeleeMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RangedMeshComponent;

	/** Worn continuously; never moved by the swap. Split out of the bow on 2026-08-01 (bow / arrow
	 *  / quiver) so the arrows stay on the goblin's back when the bow is holstered. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> QuiverMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldTorchMeshComponent;

	/** The held torch's flame. Built and torn down in lockstep with HeldTorchMeshComponent. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> HeldTorchFlameFXComponent;

	/** The war-horn. Built lazily on the first blast and kept, like the torch - a goblin that never
	 *  blows one never pays for it. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HornMeshComponent;

	/**
	 * The wheel's contents, in SECTOR ORDER starting at the top and going anticlockwise:
	 * index 0 = top, 1 = left, 2 = bottom, 3 = right.
	 *
	 * This is the thing #274 was really for. While slots were an enum, the wheel's arrangement was
	 * four hard-coded returns inside SlotForDirection and adding a fifth slot meant editing C++ in
	 * three files. Now the sector maths finds an index and this array says what lives there, so a
	 * character with a different loadout is a data change.
	 *
	 * Defaults are set in the constructor rather than here, because a native gameplay tag is not a
	 * constant expression. Order matches what the enum produced, deliberately: top Torch, left
	 * Sword, bottom Grapple, right Bow. Do not "tidy" it into declaration order - the index IS the
	 * sector.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Wheel", meta = (Categories = "WeaponSlot"))
	TArray<FGameplayTag> WheelSlots;

	// --- ACF equipment migration (ruling 53, #286) ------------------------------------------------

	/**
	 * OFF until a weapon has actually been moved onto ACF. While false, nothing here talks to
	 * UACFEquipmentComponent and the game behaves exactly as before the migration started - which is
	 * what makes each stage independently closeable and independently revertable.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|ACF")
	bool bUseACFEquipment = false;

	/**
	 * WeaponSlot (what the player CHOSE) -> ItemSlot (where the thing HANGS). Two different axes, and
	 * #274 records that conflating them would be a real mistake.
	 *
	 * A slot with NO entry is one ACF knows nothing about - the Torch and the Grapple, a held prop and
	 * a verb rather than equipped weapons. That is deliberate: ACF has no notion of a selectable slot
	 * holding no weapon, and UseEquippedItemBySlot on an empty slot does **nothing at all, silently**
	 * - no sheathe, no broadcast, no log. An unmapped slot is therefore handled by sheathing
	 * explicitly rather than by asking ACF and hoping.
	 *
	 * NEVER put a WeaponSlot tag in the VALUE column. ACF validates item slots against the `Itemslot`
	 * tag root; a WeaponSlot tag fails it, after which UseEquippedItemBySlot, IsSlotAvailable and
	 * UnequipItemBySlot all early-return at **Log** verbosity - invisible at default log levels.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|ACF")
	TMap<FGameplayTag, FGameplayTag> WeaponSlotToItemSlot;

	/** Sword by default: a goblin starts a raid with the blade out. Set in the constructor. */
	FGameplayTag CurrentSlot;

	// --- Wheel state. Purely local and input-driven; nothing here replicates. The SLOT is what
	// matters to anyone else, and it changes through SetSlot like any other path.
	bool bWheelOpen = false;
	FVector2D WheelAccum = FVector2D::ZeroVector;
	FGameplayTag WheelHighlight;

	/**
	 * How far the accumulated drag must travel before the wheel will commit to anything.
	 *
	 * In raw mouse-delta units, which are not pixels and not degrees - they are whatever the input
	 * device reports per frame, so this needs tuning against a real mouse rather than derivation.
	 * Too small and a twitch during a Q tap re-arms you mid-fight; too large and the wheel feels
	 * unresponsive. 40 is a starting guess and is EditDefaultsOnly so it can be tuned without a
	 * rebuild once the widget exists to see it against.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Weapon|Wheel", meta = (ClampMin = "1.0"))
	float WheelDeadZone = 40.f;

	bool bSwapLocked = false;
	bool bTorchReadied = false;
	bool bHornRaised = false;

public:
	/**
	 * "My owner arms me itself, later - do not complain that I start empty."
	 *
	 * AGSHordeGoblin equips DA_Weapon_HordeGoblin in its own BeginPlay, which runs AFTER this
	 * component's. The null-EquippedWeapon warning below therefore fired on every single summoned
	 * goblin and said they had no abilities and default attributes - and it was wrong every time.
	 *
	 * That false diagnostic has now caused TWO misdiagnoses (#144, #267): both times an agent read
	 * the warning, concluded the warband was unarmed, and proposed editing a working Blueprint to
	 * match it. #144 disproved it by reading EquippedWeapon off the live pawns in PIE, where every
	 * goblin reported DA_Weapon_HordeGoblin. A diagnostic that is confidently wrong for a whole
	 * class of pawn is worse than no diagnostic, because it manufactures work.
	 *
	 * Set in the OWNER'S CONSTRUCTOR, not BeginPlay - component BeginPlay runs first, so a flag set
	 * any later would arrive after the check it exists to suppress.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GoblinSiege|Weapon")
	bool bExpectsExternalEquip = false;

private:
	int32 BloodOrbs = 0;
	FTimerHandle SwapLockTimerHandle;

	/**
	 * One-shot warn latches per mesh slot, matching AGSFieldFireObjective::bSmokeSystemResolveFailed
	 * and UGSBurnMaskSubsystem's pattern: a missing soft asset says so ONCE and the failed resolve
	 * is never retried, because LoadSynchronous does not cache a failure - retrying it is a full
	 * failed package lookup, and this code runs on every swap.
	 *
	 * RESET ON EQUIP, deliberately: these are latched against the CURRENT weapon's paths, and a
	 * different data asset is entitled to a fresh attempt and a fresh warning. Latching them for
	 * the component's whole life would mean equipping a kit whose sword exists, after one whose
	 * sword doesn't, silently yields no sword.
	 */
	bool bMeleeMeshResolveFailed = false;
	bool bRangedMeshResolveFailed = false;
	bool bQuiverMeshResolveFailed = false;
	bool bHeldTorchMeshResolveFailed = false;
	bool bHeldTorchFlameResolveFailed = false;
	bool bHornMeshResolveFailed = false;

	/**
	 * Sockets already complained about, so the "hand_r_weapon does not exist" line appears once and
	 * not on every swap for the rest of the raid. Keyed by socket NAME rather than by slot, because
	 * the interesting fact is which socket the skeleton is missing - that is what someone has to go
	 * and author - and two slots pointed at the same missing socket are one problem, not two.
	 *
	 * Cleared on equip alongside the resolve latches: a new weapon may name different sockets.
	 */
	TSet<FName> WarnedMissingSockets;
};
