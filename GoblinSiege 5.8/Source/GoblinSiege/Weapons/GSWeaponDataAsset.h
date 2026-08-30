// Weapon-as-data: one asset per weapon kit (Scout sword⇄bow, Great-club, Shadow Staff, Blood
// Staff). Equip/swap is a data operation on UGSWeaponComponent - no per-weapon pawn classes
// (design doc §5). Reconstructed 2026-07-19.
//
// 2026-08-01 - THE VISUAL HALF. Until today this asset described a weapon's numbers and abilities
// and nothing at all about how it LOOKS, which is why UGSWeaponComponent equipped weapons that
// were never visible in anyone's hands: there was no mesh field to equip and no socket field to
// attach it to. The GoblinSiege|Weapon|Visual block below is that missing half, and it stays HERE
// rather than on the character because "class == weapon" (design doc §5) - a weapon's silhouette
// is part of its identity in exactly the same way its turn rate is, and the whole point of the
// data-asset design is that adding a kit means authoring an asset, not touching a pawn class.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSWeaponDataAsset.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class UStaticMesh;
class UNiagaraSystem;

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	FText DisplayName;

	/** One Damage.* tag (GSGameplayTags) - the column this weapon uses in the race matchup matrix. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float BaseDamage = 10.f;

	/**
	 * UNUSED, and deliberately left that way. Nothing reads this.
	 *
	 * Its ranged twin below had the same problem and was a real bug - the bow fired as fast as the
	 * mouse could click until #034 wired it up. Melee is different: UGSGA_SwordLight is
	 * InstancedPerActor and holds a combo stage machine, so GAS refuses re-activation while a swing
	 * is running, and the pacing a player feels is the stage timings and montage play rate. Adding a
	 * second gate here would fight the combo rather than tune it.
	 *
	 * Left in place rather than deleted because that is a designer-facing call, not a code one.
	 * If it stays, this comment is why it reads as configured but does nothing.
	 * AI melee pacing is elsewhere again: UBTTask_MeleeAttack uses GSRaceDataAsset's own
	 * AttackCooldownSeconds, which IS read.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float AttackCooldownSeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float AttackRange = 100.f;

	/** Per-weapon turn-rate identity (Brute turns like a barge - design doc "Turn rate"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon")
	float TurnRateRadPerSec = 8.f;

	/** Sets the class's Health/Armor/MoveSpeed baseline on equip (Slasher 110 HP fast, Brute
	 *  140 HP slow, Shaman 90 HP - race-design-goblins.md class table). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Init")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	/** The kit: light/heavy attack + the "E" ability, granted on equip and revoked on swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Init")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	// --- Ranged mode (the Scout's sword⇄bow toggle; template for future flex weapons) ---
	// "Slasher" corrected to "Scout" and "dagger" to "sword" on 2026-08-01 - the class was folded
	// into the Scout and the daggers into a single sword by design doc §13 decision 36, as amended
	// on 2026-08-01. Only the comments touched by this pass were corrected; there is no global
	// sweep, so other files still say Slasher.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode")
	bool bHasRangedMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	FGameplayTag RangedDamageTypeTag;

	/** Ranged trades damage-per-second for range/safety (race-design-goblins.md swap rule). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedBaseDamage = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedAttackCooldownSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	float RangedAttackRange = 900.f;

	/**
	 * The item a shot from this bow SPENDS. Null means this bow's shots are free (ruling 46, #280).
	 *
	 * NULL MEANS INFINITE, NOT BROKEN, and that direction is deliberate: a mis-authored asset costs
	 * unlimited arrows, never a dead bow. The opposite default has bitten this project twice, where a
	 * null class silently disabled a verb instead of loosening it.
	 *
	 * It lives HERE rather than on UGSGA_BowShot because there is no GA_GS_BowShot Blueprint - a
	 * property on the ability would only ever hold its C++ default, and reaching it would need either
	 * a ConstructorHelpers path into content or a new Blueprint plus a change to the ability grant.
	 * The ability already reads this asset for RangedAttackCooldownSeconds, and "which ammo" is a
	 * property of the bow rather than of the act of shooting.
	 *
	 * SET ON THE PLAYER'S BOW ONLY. DA_Weapon_Erika deliberately leaves it null - see
	 * UGSGA_BowShot::GetAmmoItemClass for the other half of what keeps AI archers free.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|RangedMode",
		meta = (EditCondition = "bHasRangedMode"))
	TSubclassOf<class UACFItem> ArrowItemClass;

	// ------------------------------------------------------------------- visuals (2026-08-01)
	/**
	 * WHY STATIC MESHES, DELIBERATELY.
	 *
	 * Every one of these is a UStaticMesh and not a USkeletalMesh, and that is a decision and not
	 * an oversight. A sword, a bow, a quiver and a torch are RIGID PROPS: none of them has a
	 * skeleton, none of them needs to deform, and none of them animates independently of the hand
	 * carrying it. A UStaticMeshComponent snapped to a socket costs one attached primitive and one
	 * transform update per frame; the alternatives are strictly worse for the same picture -
	 * a USkeletalMeshComponent drags an animation evaluation and a bone hierarchy behind it for a
	 * mesh with one bone, and a ChildActorComponent drags a whole actor, its own registration
	 * lifetime and a spawn/destroy round-trip through the world on every weapon swap.
	 *
	 * The one thing static meshes cannot do is bend, and the only kit member that arguably wants
	 * to is the bow's string on a full draw. That is a shader/scale trick when it matters, not a
	 * reason to pay skeletal cost on all four props today.
	 *
	 * NOTHING HERE MAY BE A HARD REFERENCE. Every mesh below is a TSoftObjectPtr resolved lazily by
	 * UGSWeaponComponent, warned about exactly once on failure, and the game must play correctly
	 * with every single one of them unset - which is the state the project is in as this is
	 * written: the sword mesh is not modelled (design doc §13 decision 36 replaced the dual daggers
	 * and the replacement does not exist yet), and the goblin skeleton has no weapon sockets on it
	 * at all. A hard reference would turn "the art isn't in yet" into "the data asset fails to
	 * load", and the whole kit would take the character down with it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> MeleeMesh;

	/** Bone/socket on the owning character's GetMesh() the melee weapon rides in while ACTIVE.
	 *  Right hand by convention - the Scout's sword is a right-handed weapon, and the bow (below)
	 *  goes to the left so the two never contend for the same socket during a live swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName MeleeSocket = FName("hand_r_weapon");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> RangedMesh;

	/** Where the bow sits while ACTIVE. Left hand: a bow is held in the off hand and drawn with the
	 *  other, so this is both anatomically right and mechanically convenient - the melee half can
	 *  stay attached to its own socket instead of being detached and reattached on every swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName RangedSocket = FName("hand_l_weapon");

	/**
	 * The quiver. WORN CONTINUOUSLY - it is not part of the sword⇄bow swap and never moves between
	 * sockets, because a Scout who has sheathed the bow is still carrying the arrows.
	 *
	 * This is the third of the three meshes the bow was split into (bow / arrow / quiver, tech doc
	 * §16 kit). The ARROW is deliberately absent from this asset: it is a projectile mesh owned by
	 * the shot that spawns it, not a thing hanging off the character, and it belongs on the future
	 * arrow projectile actor exactly as the torch's flight mesh belongs on AGSTorchProjectile.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> QuiverMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName QuiverSocket = FName("spine_quiver");

	/**
	 * Where the UNEQUIPPED half of the sword⇄bow pair sits. The swap is live (design doc §5,
	 * race-design-goblins.md swap rule) and both halves are carried at all times, so the inactive
	 * one has to be SOMEWHERE - and "back" is the answer that keeps the silhouette readable at the
	 * distance the camera actually sits at (tech doc §16: ~450uu arm on a 240uu goblin).
	 *
	 * Two separate sockets rather than one shared "back" socket, because a sword and a bow hang at
	 * completely different angles and a single socket would need the offsets below to fight it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName MeleeHolsterSocket = FName("back_sword");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName RangedHolsterSocket = FName("back_bow");

	/**
	 * PER-WEAPON FUDGE, applied on top of the socket snap.
	 *
	 * Socket authoring never lines up first time: a mesh is exported with whatever origin and axis
	 * convention the modelling package felt like, and the socket is placed by eye on a bone in a
	 * skeleton that was retargeted from something else. Without these, every "the sword is 3cm
	 * inside his palm and rolled 90°" fix is a Blender round-trip - re-origin, re-export, re-import,
	 * re-check - for a number that is known within ten seconds of looking at it in the editor.
	 * With these it is a drag in the details panel while PIE is running.
	 *
	 * Identity (the default) means "trust the socket exactly", which is what a correctly authored
	 * socket/mesh pair should need. Treat a large value here as a note to fix the source asset
	 * eventually, not as a permanent home for the transform.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform MeleeMeshOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform RangedMeshOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform QuiverMeshOffset;

	/**
	 * Whether the holstered half is drawn at all. True by default: seeing the bow on his back while
	 * the sword is out is most of what sells "this goblin carries a kit", and it is the cheap half
	 * of the silhouette work.
	 *
	 * Set it false per weapon for kits whose second half has no back-carry pose authored yet - a
	 * bow clipping through a shoulder is worse than a bow that isn't there, and this is the switch
	 * that buys time without deleting the mesh reference.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	bool bShowHolsteredWeapon = true;

	/**
	 * THE HELD TORCH - the lit torch in the goblin's hand before it is thrown.
	 *
	 * It lives on the WEAPON data asset, not on UGSGA_TorchToss, and that was the pick over putting
	 * it on the ability (decided 2026-08-01):
	 *
	 *  - Tech doc §16's tool-swap scheme makes the torch an ACTIVE TOOL SLOT alongside
	 *    Melee/Ranged/Horn, driven by a toggle that explicitly "generalizes
	 *    UGSWeaponComponent::ToggleRangedMode()'s existing toggle-and-timer-lock shape rather than
	 *    inventing a second equip mechanism". The torch's visual therefore belongs to the same
	 *    owner as the other tool slots' visuals, or the Q toggle would have to reach into an
	 *    ability to move a mesh.
	 *  - A UGameplayAbility is a poor owner for a persistent scene component. It can be cancelled,
	 *    interrupted, re-instanced, or activated while a previous instance is still ending, and it
	 *    has no EndPlay - so a mesh parented to it has no single reliable place to be destroyed.
	 *    UGSWeaponComponent has exactly one lifetime, one EndPlay, and already owns socket
	 *    resolution, the missing-socket fallback, the offset application and the warn-once latches
	 *    that this mesh needs. Duplicating that machinery inside the ability to hold one prop would
	 *    be the whole of this pass written twice.
	 *  - The torch is universal (every goblin throws torches regardless of kit), so this field
	 *    being per-weapon-asset is a mild redundancy - the same torch mesh gets set on every kit's
	 *    asset. That is the accepted cost: it buys a single attachment code path, and it leaves the
	 *    door open for a kit-specific torch (the Shaman's brand, say) without new code.
	 *
	 * Shown only while the torch is READIED (UGSWeaponComponent::SetTorchReadied) and hidden the
	 * instant it is thrown, at which point the thrown prop is AGSTorchProjectile's own TorchMesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> HeldTorchMesh;

	/** Off hand, on its own socket rather than sharing RangedSocket: the readied torch and the bow
	 *  can never be up at the same time (one active tool slot), but co-locating them on one socket
	 *  would force one authored pose to serve a bow grip and a torch grip, which it cannot. Author
	 *  this on the same bone as hand_l_weapon if that turns out to be right - the point is that it
	 *  is a separate, separately-tweakable transform. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName HeldTorchSocket = FName("hand_l_torch");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform HeldTorchMeshOffset;

	/**
	 * The flame at the head of the held torch. Soft and optional, same degrade-not-crash rule as
	 * every other FX reference in this project (see AGSFireVolume::FireSystem): a missing system
	 * costs the flame, not the torch - the mesh alone is still a legible held prop.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UNiagaraSystem> HeldTorchFlameSystem;

	/** Offset from HeldTorchSocket to the flame. Separate from HeldTorchMeshOffset on purpose - the
	 *  flame sits at the torch's burning tip, which is almost never the mesh's own pivot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform HeldTorchFlameOffset;

	/**
	 * The war-horn prop (GDD 2.3 "universal kit, every class").
	 *
	 * Same shape as HeldTorchMesh above and the same accepted redundancy: the horn is universal, so
	 * this ends up set to the same mesh on every kit's asset. That buys one attachment code path and
	 * leaves the door open for a kit-specific horn without new code.
	 *
	 * Shown only while the horn is RAISED (UGSWeaponComponent::SetHornRaised), which UGSGA_Horn
	 * drives for the length of the blast. It is not a slot and it is not equippable - the horn is
	 * something a goblin has, not something it swaps to.
	 *
	 * LIVES HERE UNDER PROTEST, and the protest is worth writing down. ACF's Inventory System owns
	 * equipment slots and attach sockets, and Phase 2 will reach equipment; when it does, this trio
	 * is expected to be deleted and re-expressed in ACF's terms. Michael ruled on 2026-08-21 to
	 * attach it our way now rather than wait, with the rework understood and accepted. Do not
	 * "discover" this later and treat it as an oversight.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> HornMesh;

	/**
	 * Left hand, on its own socket - hand_l_horn, authored on GOB_Scout_v2 on 2026-08-21.
	 *
	 * Every one of the six pre-existing sockets was already spoken for: hand_r_weapon is the melee
	 * blade, hand_l_weapon is the drawn bow, hand_l_torch is the readied torch, and the three back_*
	 * sockets are holsters. Reusing any of them would have put two props in one hand the first time
	 * a player blew the horn with a torch up.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FName HornSocket = FName("hand_l_horn");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Weapon|Visual")
	FTransform HornMeshOffset;
};
