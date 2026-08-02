// Shared base for every goblin, human, elf, and dwarf pawn. Owns the AbilitySystemComponent and
// AttributeSet so damage, armor, and status effects (root/stun/burn) work identically across
// player-controlled goblins and AI-controlled defenders - only the granted abilities and data
// assets differ per subclass. See design doc §10 "Road to Unreal" table.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GSCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGSAttributeSetBase;
class UGameplayEffect;
struct FOnAttributeChangeData;

UCLASS(Abstract)
class GOBLINSIEGE_API AGSCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGSCharacterBase();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	UGSAttributeSetBase* GetAttributeSetBase() const { return AttributeSetBase; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Combat")
	bool IsAlive() const { return !bIsDead; }

	/** Applied on respawn: sets Health to RespawnHealthFraction * MaxHealth and grants brief i-frames. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Combat")
	virtual void ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds);

	/** Per-archetype/per-weapon turn-rate identity (Brute turns like a barge, Slasher/Scout turns
	 *  sharp - design doc "Turn rate"). Pushes the value into CharacterMovementComponent::RotationRate
	 *  so bOrientRotationToMovement-driven turning actually uses it. Called by UGSWeaponComponent on
	 *  equip so a weapon's identity applies itself to its wearer (tech doc §16). */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Movement")
	void SetTurnRateRadPerSec(float NewTurnRateRadPerSec);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Movement")
	float GetTurnRateRadPerSec() const { return TurnRateRadPerSec; }

protected:
	virtual void BeginPlay() override;

	/** Grants the base GameplayEffect (Health/MaxHealth/Armor/MoveSpeed init values) from data. Called
	 *  by subclasses after their race/weapon data asset is known. */
	void InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level = 1.f);

	/** Bound to the Health attribute's OnAttributeChanged delegate in BeginPlay. */
	virtual void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** Called once when Health first reaches 0. Notifies GSGameMode::HandleGoblinDeath. */
	virtual void HandleDeath();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GoblinSiege|Abilities")
	TObjectPtr<UGSAttributeSetBase> AttributeSetBase;

	/** Turn rate in rad/s - per-archetype tuning knob called out repeatedly in the design doc
	 *  (Brute 5, Slasher 12, Shaman 10, Militia/Knight slower still). Drives
	 *  CharacterMovementComponent::RotationRate via SetTurnRateRadPerSec rather than a snap-to-facing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GoblinSiege|Movement")
	float TurnRateRadPerSec = 8.f;

	UPROPERTY(BlueprintReadOnly, Category = "GoblinSiege|Combat")
	bool bIsDead = false;

	bool bAttributesInitialized = false;

	// ---- Death presentation (2026-08-02) -------------------------------------------------
	// Ragdoll moved from "skip" to "adopt" - the physics comedy is on-brand for a comedy game
	// (Michael's ruling, see claude/goblin-siege-acf-integration-plan.md). This does NOT replace
	// UGSGibComponent when that lands: gib is for lethal overkill, ragdoll for ordinary death.
	// Exposed as EditDefaultsOnly rather than hard-coded because death feel is a tuning pass, and
	// with Live Coding unable to add UPROPERTYs, a knob you forgot costs a full rebuild.

	/** Simulate physics on the mesh when Health hits 0. Off for anything that should stay standing. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	bool bRagdollOnDeath = true;

	/** Collision profile applied to the mesh before simulating. "Ragdoll" is the engine default. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** Extra shove along the killing blow's direction, so a corpse sells the hit. 0 = limp drop. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death")
	float DeathImpulse = 0.f;

	/** Seconds before the corpse is destroyed. 0 = never (correct for a playtest - you want to see
	 *  what you killed). Set non-zero once a raid has enough bodies to matter. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Combat|Death", meta = (ClampMin = "0.0"))
	float CorpseLifespan = 0.f;
};
