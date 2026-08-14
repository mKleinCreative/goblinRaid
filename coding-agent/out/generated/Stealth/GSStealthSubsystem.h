// The one funnel between "a guard confirmed a goblin" and the rest of the game (design doc §2.4,
// §2.6): it de-duplicates confirms into exactly one soft signal, hands that signal to
// AGSGameState::ReportConfirmedSighting() so the town moves to Suspicious, and remembers whether
// anyone confirmed a goblin before the first objective caught fire ("First Spark Unseen", §2.4).
// 2026-08-04 (Block D): also owns the stealth-target registry and stance resolution so the
// perception component never has to sweep the world.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Stealth/GSStealthTypes.h"
#include "GSStealthSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSOnStealthConfirmSignal, AActor*, Observer, AActor*, Target, int32, TotalConfirms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnFirstSparkResolved, bool, bUnseen);

/** Non-reflected key: which observer confirmed which target. Weak so it never keeps actors alive. */
struct FGSConfirmKey
{
	TWeakObjectPtr<const AActor> Observer;
	TWeakObjectPtr<const AActor> Target;

	bool operator==(const FGSConfirmKey& Other) const
	{
		return Observer == Other.Observer && Target == Other.Target;
	}

	friend uint32 GetTypeHash(const FGSConfirmKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Observer), GetTypeHash(Key.Target));
	}
};

UCLASS()
class GOBLINSIEGE_API UGSStealthSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth", meta = (WorldContext = "WorldContextObject"))
	static UGSStealthSubsystem* Get(const UObject* WorldContextObject);

	// ---------------------------------------------------------------- target registry

	/** Anything a guard should be able to confirm. Player pawns are picked up automatically;
	 *  AI goblins and other sneaking actors register here. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void RegisterStealthTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void UnregisterStealthTarget(AActor* Target);

	/** Fills Out with every live stealth target: explicit registrations plus every player pawn. */
	void GatherStealthTargets(TArray<AActor*>& OutTargets) const;

	// ---------------------------------------------------------------- stance

	/** Crouch comes straight off ACharacter unless something set an explicit override. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	EGSStealthStance GetStanceFor(const AActor* Target) const;

	/** Force a stance - used for downed/dead actors (Hidden) and for AI that fakes a crouch. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void SetStanceOverride(AActor* Target, EGSStealthStance Stance);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void ClearStanceOverride(AActor* Target);

	// ---------------------------------------------------------------- confirms

	/**
	 * Server-side. A guard's 1.5s hold landed. Emits exactly one soft signal into the alarm
	 * system (AGSGameState::ReportConfirmedSighting -> Quiet becomes Suspicious) and counts the
	 * confirm for First Spark Unseen. Repeat calls for the same observer/target pair inside
	 * ConfirmDedupeSeconds are swallowed. Returns true if a signal was actually emitted.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	bool ReportSightingConfirmed(AActor* Observer, AActor* Target);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	int32 GetConfirmedSightingCount() const { return ConfirmedSightingCount; }

	// ---------------------------------------------------------------- first spark unseen

	/** Server-side. Call the moment the first objective/structure ignites. Idempotent: the first
	 *  call snapshots whether any goblin had been confirmed up to that point. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Stealth")
	void NotifyFirstObjectiveIgnited();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	bool HasFirstSparkHappened() const { return bFirstSparkHappened; }

	/** Before the first fire: is the bonus still live? After it: did it pay out? */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Stealth")
	bool IsFirstSparkUnseen() const { return bFirstSparkHappened ? bFirstSparkUnseen : (ConfirmedSightingCount == 0); }

	/** Broadcast once per emitted soft signal. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stealth")
	FGSOnStealthConfirmSignal OnStealthConfirmSignal;

	/** Broadcast once, when the first objective ignites, with the bonus verdict. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Stealth")
	FGSOnFirstSparkResolved OnFirstSparkResolved;

	/** Window in which a repeat confirm from the same observer on the same target is silent. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|Stealth", meta = (ClampMin = "0.0"))
	float ConfirmDedupeSeconds = 8.f;

private:
	/** Weak so unregistering is optional - dead actors fall out on the next gather. */
	TSet<TWeakObjectPtr<AActor>> RegisteredTargets;

	TMap<TWeakObjectPtr<const AActor>, EGSStealthStance> StanceOverrides;

	TMap<FGSConfirmKey, double> LastConfirmTimes;

	int32 ConfirmedSightingCount = 0;

	bool bFirstSparkHappened = false;

	bool bFirstSparkUnseen = false;
};
