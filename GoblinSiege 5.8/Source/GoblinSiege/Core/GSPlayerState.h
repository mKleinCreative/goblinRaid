// Per-goblin state: the 5-lives system, independently tracked per player/companion (design doc
// §3). Lives live HERE, not on the pawn - pawns die and respawn, PlayerState persists.
// Reconstructed 2026-07-19.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GSPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnLivesChanged, int32, LivesRemaining);

UCLASS()
class GOBLINSIEGE_API AGSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGSPlayerState();

	/** Server-only. Spends one life; returns lives remaining after the loss. */
	int32 LoseLife();

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Lives")
	void ResetLivesForNewRaid();

	/** DEBUG ONLY. Drop straight to the last life so OutOfLives can be reached in one death rather
	 *  than five, which is the difference between a testable lose path and an untested one. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Lives|Debug")
	void DebugSetLives(int32 NewLives);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Lives")
	int32 GetLives() const { return Lives; }

	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Lives")
	FGSOnLivesChanged OnLivesChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Lives();

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Lives")
	int32 MaxLives = 5;

	UPROPERTY(ReplicatedUsing = OnRep_Lives)
	int32 Lives = 5;
};
