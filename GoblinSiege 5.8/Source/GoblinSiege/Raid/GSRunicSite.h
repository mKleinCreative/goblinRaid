// The way out (design doc §2.1, §9): the runic site the raid spawns at, respawns at, and extracts
// through. Written 2026-08-05 with UGSRaidDirector.
//
// AN AUTO-BANK CIRCLE, NOT A HOLD-E CHANNEL. This is why GSTags::Interact_Extract has sat declared
// and deliberately unused since 2026-08-04 - the ruling was that extraction happens by standing in
// the circle (GDD §9), and asking a player to hold a key while a portal collapses around them
// fights the moment rather than serving it. If that ever flips, the tag is already there.
//
// The actor owns STATE and OVERLAP only. Mesh and Niagara are assigned on BP_GS_RunicSite, which
// composes the Portal 4 set that already ships in Content/PortalVFXEnhanced:
//     SM_Portal4 / M_Portal4 / N_Portal4_V2
// No VFX is authored here; the pack has no Blueprints, so the Blueprint is the composition step.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSRunicSite.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnPortalOpenChanged, bool, bIsOpen);

UCLASS()
class GOBLINSIEGE_API AGSRunicSite : public AActor
{
	GENERATED_BODY()

public:
	AGSRunicSite();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Runic Site")
	bool IsPortalOpen() const { return bPortalOpen; }

	/**
	 * Server-only. Normally driven by UGSRaidDirector::OnRaidObjectivesComplete, which this actor
	 * binds itself to - a designer never has to wire it. BlueprintCallable anyway so a tutorial
	 * script or a debug command can force it.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Runic Site")
	void SetPortalOpen(bool bOpen);

	/** Fires on server and clients (via OnRep). Blueprint hook for audio, camera shake, a
	 *  screen-edge glow - anything the C++ has no business deciding. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Runic Site")
	FGSOnPortalOpenChanged OnPortalOpenChanged;

	/** Where AGSGameMode::ChoosePlayerStart sends a respawning goblin (GDD §1: "respawn is at the
	 *  runic site"). Offset from the portal plane so a respawn does not immediately re-trigger
	 *  extraction on an open portal. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Runic Site")
	FTransform GetSpawnTransform() const;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleObjectivesComplete();

	UFUNCTION()
	void OnRep_PortalOpen();

	/** Mesh visibility + Niagara activation from bPortalOpen. Runs on both sides so a listen
	 *  server and its client agree without a second replicated field. */
	void ApplyPortalVisuals();

	/**
	 * Try to extract whoever is already inside the circle.
	 *
	 * Called when the portal OPENS, not only on overlap - and this is a real case, not defensive
	 * padding. Torch-toss lets a player finish the last objective from range, and the tutorial's
	 * runic site sits close enough to the farm that "burn the field from the portal steps" is a
	 * perfectly ordinary thing to do. Without this, the player stands in an open portal and
	 * nothing happens until they walk out and back in.
	 */
	void TryExtractOverlappingPawns();

	/** Returns true if this actor is a pawn a human is driving. Race-agnostic on purpose - a
	 *  possessed horde goblin should extract the same way the Scout does. */
	static bool IsExtractablePawn(const AActor* Actor);

	void TryExtract(AActor* OtherActor);

	// ------------------------------------------------------------------ components

	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Runic Site")
	TObjectPtr<USceneComponent> SiteRoot;

	/** The auto-bank circle. Generous by design: this is the "you made it" moment, and a portal
	 *  you can clip past at a sprint is a bug report. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Runic Site")
	TObjectPtr<USphereComponent> ExtractionSphere;

	/** SM_Portal4 on the Blueprint. Hidden until the portal opens. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Runic Site")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	/** N_Portal4_V2 on the Blueprint. bAutoActivate is forced off here - a portal that is already
	 *  blazing when the raid starts tells the player the exit is open when it is not. */
	UPROPERTY(VisibleAnywhere, Category = "GoblinSiege|Runic Site")
	TObjectPtr<UNiagaraComponent> PortalFX;

	// ------------------------------------------------------------------ tuning

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Runic Site|Tuning", meta = (ClampMin = "100.0"))
	float ExtractionRadius = 1200.f;

	/** How far in front of the portal plane a respawn lands. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Runic Site|Tuning")
	float SpawnForwardOffset = 900.f;

	/**
	 * If true, the site opens itself the moment every objective type has burned. Off would mean
	 * something else (a cutscene, a courier, a tutorial beat) owns the moment.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Runic Site|Tuning")
	bool bOpenOnObjectivesComplete = true;

	/** Start open. Debug and tutorial-of-the-tutorial use; normally false. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Runic Site|Tuning")
	bool bStartOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_PortalOpen)
	bool bPortalOpen = false;
};
