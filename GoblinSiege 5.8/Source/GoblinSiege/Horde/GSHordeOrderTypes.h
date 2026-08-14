// The four verbs of the point command (GDD §2.5), and the standing-order record UGSHordeSubsystem
// keeps per summoner. Written 2026-08-12, ticket #141.
//
// WHY A SEPARATE HEADER: EGSHordeOrder is needed by UGSHordeSubsystem, AGSHordeAIController,
// AGSHordeOrderMarker, UGSHordeCommandComponent, UGSHordeOrderWheelWidget and three BT tasks.
// Declaring it on the subsystem would make a widget that draws four labels include the whole horde
// pool, its config knobs and its threat registry - the same reason EGSWeaponSlot does not live on
// AGSPlayerCharacter.
//
// GDD §2.5 specifies ONE context command ("aim at an enemy - get 'em; at a prop - smash it; at loot
// or a pig - courier it home") and no squad layer. This is that command plus a recall, made explicit
// on a wheel at Michael's direction (2026-08-12): the context resolution still happens - Attack on a
// breakable smashes rather than swings - but the player names the verb rather than hoping the game
// guessed right off his crosshair.
#pragma once

#include "CoreMinimal.h"
#include "GSHordeOrderTypes.generated.h"

class AGSHordeOrderMarker;

/**
 * What the warband has been told to do.
 *
 * DECLARATION ORDER IS NOT WHEEL ORDER, and must not be "tidied" to match it. This is written to
 * BB_HordeGoblin as a byte, so renumbering silently repoints every saved decorator in the tree at a
 * different verb - the same trap EGSAlarmSource carries ("appended, never inserted"). The wheel's
 * geometry lives in UGSHordeCommandComponent::OrderForDirection and nowhere else.
 *
 * None is 0 deliberately: an un-commanded goblin reads the blackboard key's zero default and needs
 * no initialisation pass, and a wheel released inside the dead zone resolves here and issues nothing.
 */
UENUM(BlueprintType)
enum class EGSHordeOrder : uint8
{
	/** No standing order. The horde is back on Follow-and-Frenzy, which is the default behaviour. */
	None    UMETA(DisplayName = "No Order"),

	/** Swarm the subject if it is alive, smash it if it is breakable. */
	Attack  UMETA(DisplayName = "Attack"),

	/** Plant at the marker and stay there. Still fights back; never chases. */
	Hold    UMETA(DisplayName = "Hold"),

	/** Recall - clears the standing order rather than setting one. */
	Follow  UMETA(DisplayName = "Follow"),

	/** Shoulder the subject and courier it home (§2.7). */
	Loot    UMETA(DisplayName = "Loot")
};

/**
 * One summoner's standing order.
 *
 * DELIBERATELY NOT A USTRUCT, and the pointers are weak for the same reason. These live in a bare
 * TMap on UGSHordeSubsystem, and a TMap that is not itself a UPROPERTY does not get its contents
 * traced by the garbage collector - a TObjectPtr in here would look like a hard reference, be treated
 * as one by every reader, and dangle anyway the first time the subject was destroyed. Weak pointers
 * make the lifetime honest: an order whose subject has gone is an order that has expired, which is
 * exactly what PruneStaleOrders acts on.
 */
struct FGSHordeOrder
{
	EGSHordeOrder Verb = EGSHordeOrder::None;

	/** The latched trace hit, or null when the player pointed at bare ground (a valid Hold order). */
	TWeakObjectPtr<AActor> Subject;

	/** Ground-corrected server-side through UGSRaidLibrary::FindStandableSpotNear. */
	FVector Location = FVector::ZeroVector;

	/** Where a courier takes its cargo. RESOLVED ONCE, HERE, at issue time - not per goblin per
	 *  refresh. Finding it iterates the world for a runic site, and AGSHordeAIController's header
	 *  forbids a world iteration at blackboard-refresh rate outright. */
	FVector DeliveryLocation = FVector::ZeroVector;

	/** The beacon the player can see. The level owns its lifetime; this only needs to find it again
	 *  to retire it. */
	TWeakObjectPtr<AGSHordeOrderMarker> Marker;

	float IssuedTime = 0.f;
};
