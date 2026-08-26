// The raid loop: what starts a raid, what wins one, and what ends one.
// Written 2026-08-05. This is the subsystem AGSBurnObjectiveBase's Q-37 hook comment asks for by
// name ("that wants a subsystem holding the per-type completion set, and it belongs to Raid Loop").
//
// Before this class, three things had no caller anywhere in C++ and the game could not be finished
// on any map, including the tutorial:
//   - AGSGameState::StartRaidClock()      - the 30-minute clock never started
//   - AGSMissionObjective::BeginObjective() - placed mission objectives never activated
//   - the Q-32 demotion pass + win check  - a 21-line comment where the code should be
//
// WHY A WORLD SUBSYSTEM rather than work on AGSGameMode:
//   1. It needs no level authoring. A world gets one of these automatically, which is what lets a
//      freshly built map be playable without a designer remembering to wire a level Blueprint -
//      and what will let a generated map be playable with nobody wiring anything at all.
//   2. The GameMode is a Blueprint asset set project-globally (DefaultEngine.ini ->
//      BP_GSGameMode), so every behaviour change here would mean editing a binary asset a designer
//      also owns.
//   3. The GameMode does not exist on clients. The HUD's objective list needs to enumerate
//      carriers from a client, and this can answer that.
//   4. UGSBurnMaskSubsystem already made exactly this call for exactly this lifecycle reason - see
//      its header on OnWorldBeginPlay vs Initialize. One pattern, not two.
//
// Config = Game: a subsystem has no editor-visible CDO, so Config is its EditDefaultsOnly. Tuning
// lives in DefaultGame.ini under [/Script/GoblinSiege.GSRaidDirector].
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Alarm/GSAlarmTypes.h"
#include "Raid/GSRaidTypes.h"
#include "GSRaidDirector.generated.h"

class AGSBurnObjectiveBase;
class UGSTopplableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnRaidEnded, EGSRaidResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnRaidObjectivesComplete);

/** One line the HUD can put on screen when something is achieved. FText because it is shown to a
 *  player, not logged. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSOnObjectiveAnnounced, const FText&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnObjectiveRosterChanged);

/**
 * All carriers of one burn TYPE, plus whether that type has been satisfied.
 *
 * Plain struct, not a USTRUCT: nothing here is replicated, serialised or Blueprint-visible, and
 * TWeakObjectPtr is safe without reflection. A carrier that is destroyed mid-raid simply goes
 * stale and is skipped, which is the behaviour we want and would otherwise have to write.
 */
struct FGSObjectiveTypeBucket
{
	TArray<TWeakObjectPtr<AGSBurnObjectiveBase>> Carriers;

	/** True once ENOUGH carriers of this type have completed. Terminal - a type is never un-won. */
	bool bTypeComplete = false;

	/** How many carriers of this type have completed. Counted rather than inferred, because a
	 *  carrier destroyed mid-raid would otherwise silently reduce the tally that already satisfied
	 *  the type. */
	int32 CompletedCount = 0;

	/** How many are needed. Resolved once at registration from the director's fractions. */
	int32 RequiredCount = 1;
};

UCLASS(Config = Game)
class GOBLINSIEGE_API UGSRaidDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UGSRaidDirector();

	// ------------------------------------------------------------------ lifecycle

	/** Game and PIE only - UE spins up editor, preview and thumbnail worlds constantly and none of
	 *  them should start a raid clock. Unlike UGSBurnMaskSubsystem this DOES exist on a dedicated
	 *  server: that subsystem is cosmetic, this is the game. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * The whole bootstrap, in one function on purpose: a map gets all of it or none of it. A level
	 * that starts its clock but never activates its mission objectives is a worse failure than one
	 * that plainly does nothing, because it looks like it is working.
	 *
	 * OnWorldBeginPlay rather than Initialize for the reason UGSBurnMaskSubsystem documents: level
	 * actors are not reliably present during world initialisation, and an objective sweep that runs
	 * too early finds an empty world and reports an unwinnable map.
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	/** Never null on a game world, but every caller should still be null-safe - that is the same
	 *  contract UGSBurnMaskSubsystem::Get sets, and it costs nothing to keep them identical. */
	static UGSRaidDirector* Get(const UObject* WorldContextObject);

	// ------------------------------------------------------------------ carrier roster

	/**
	 * Idempotent. Called twice by design: once by the world sweep in OnWorldBeginPlay, and once by
	 * each carrier from its own BeginPlay.
	 *
	 * Doing both is not belt-and-braces for its own sake. The ordering between
	 * UWorldSubsystem::OnWorldBeginPlay and actor BeginPlay is not contractually fixed across
	 * engine versions, and the self-announce is also the ONLY way a carrier spawned mid-raid can
	 * join the roster - which the Q-37 hook comment calls out explicitly as a requirement ("gives
	 * late-spawned carriers no way to learn they were born Optional"). The sweep alone cannot do
	 * that; the announce alone would depend on ordering luck.
	 *
	 * This is not the registry the FindObjectiveAtLocation comment argues against. That argument is
	 * about putting an O(n) level scan on a gameplay beat - the torch throw. This runs once per
	 * carrier per world.
	 */
	void RegisterCarrier(AGSBurnObjectiveBase* Carrier);

	/** Snapshot for the HUD. Stale/destroyed carriers are filtered out here rather than by callers. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid")
	void GetObjectiveRows(TArray<FGSObjectiveRow>& OutRows) const;

	/** Live carriers in registration order. The HUD binds each one's own state delegates. */
	void GetTrackedCarriers(TArray<AGSBurnObjectiveBase*>& OutCarriers) const;

	// ------------------------------------------------------------------ raid state

	/**
	 * Server-only, and terminal: the FIRST result wins and later calls are ignored. That ordering
	 * matters - a goblin who spends their last life *while stepping through an open portal* has
	 * extracted, and whichever call lands first is the one the player experienced.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|Raid")
	void EndRaid(EGSRaidResult Result);

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	EGSRaidResult GetRaidResult() const { return RaidResult; }

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	bool HasRaidEnded() const { return RaidResult != EGSRaidResult::NotEnded; }

	/** One of every placed type has burned - the win condition. The runic site binds this to open. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	bool AreObjectivesComplete() const { return bObjectivesComplete; }

	/** Types the raid needs one burn of. Seeded from what was actually PLACED, so a future
	 *  generator's three-objective roll is expressed by the level rather than by a second list
	 *  somewhere that can disagree with it. */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	int32 GetRequiredTypeCount() const { return RequiredTypes.Num(); }

	/**
	 * How many carriers of this type must complete. 0 for a type the level does not have.
	 *
	 * EXISTS FOR THE HUD, and for a reason worth stating: the collapsed row counted GROUP SIZE, so
	 * with a 40% house rule the player was told "Houses 0/67" when 27 would do. A requirement the
	 * player is told wrong is worse than one they are not told at all - they budget the whole raid
	 * against it.
	 */
	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	int32 GetRequiredCountForType(FGameplayTag TypeTag) const;

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|Raid")
	int32 GetCompletedTypeCount() const;

	// ------------------------------------------------------------------ events

	/** Server-side. Fires exactly once per raid. The seam the score screen and post-raid
	 *  progression hang off later. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Raid")
	FGSOnRaidEnded OnRaidEnded;

	/** Server-side. AGSRunicSite binds this to open its portal. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Raid")
	FGSOnRaidObjectivesComplete OnRaidObjectivesComplete;

	/**
	 * Something worth telling the player about happened.
	 *
	 * Michael, 2026-08-25, asked for "prompts for when an objective gets completed". Deliberately ONE
	 * delegate carrying a finished line rather than a family of typed events: the HUD's job is to show
	 * a sentence, and every caller here already knows which sentence it means. A HUD that had to
	 * assemble the wording would be a second place the rules live.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Raid")
	FGSOnObjectiveAnnounced OnObjectiveAnnounced;

	/** A carrier joined the roster - the HUD rebuilds its list. Fires on clients too. */
	UPROPERTY(BlueprintAssignable, Category = "GoblinSiege|Raid")
	FGSOnObjectiveRosterChanged OnObjectiveRosterChanged;

protected:
	/** The Q-32/Q-37 demotion pass and the win check. Bound to every carrier's completion
	 *  delegate, so it never polls - which is what the hook comment specified. */
	UFUNCTION()
	void HandleCarrierCompleted(AGSBurnObjectiveBase* Objective);

	UFUNCTION()
	void HandleRaidClockPhaseChanged(EGSRaidClockPhase NewPhase);

	void EvaluateWinCondition();

	/** NM_Client is the only net mode that is not authoritative. Standalone counts as authority,
	 *  which is what the single-player slice runs as. */
	bool HasAuthority() const;

	// ------------------------------------------------------------------ tuning

	/** Off only for a test map that wants to drive the clock itself. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GoblinSiege|Raid|Tuning")
	bool bAutoStartRaidClock = true;

	/** Also call BeginObjective() on placed AGSMissionObjectives at raid start. Separate from the
	 *  clock switch because the two failure modes are unrelated. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GoblinSiege|Raid|Tuning")
	bool bAutoBeginMissionObjectives = true;

	/**
	 * What FRACTION of a type's carriers must burn before the type counts, keyed by type tag.
	 *
	 * SUPERSEDES RULING Q-32 (2026-07-31), which said the first carrier of a type to burn demotes
	 * every sibling to Optional. That was right when a type meant "the mill" or "the market" - one
	 * building, one objective. It is wrong for houses: Michael, 2026-08-25, wants "a percentage of
	 * houses", and under Q-32 burning a single cottage satisfied all 67.
	 *
	 * A PERCENTAGE RATHER THAN A COUNT, and the reason is visible in play: two of Tutorial Island's
	 * village houses are each split into two objectives (one per storey), so "burn 8 houses" is a
	 * number the player can watch be wrong. A fraction absorbs that.
	 *
	 * Types absent from this map need ONE carrier, which is the old behaviour - so the market, the
	 * field and every future one-of-a-kind objective are unaffected and need no entry.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GoblinSiege|Raid|Tuning")
	TMap<FGameplayTag, float> TypeCompletionFraction;

	/**
	 * Types that register, list and score but never gate the win.
	 *
	 * The mill lives here. Michael listed the required set as "a percentage of houses, the market
	 * stalls, the Statue and the field. Everything else is optional" - and the mill is placed and
	 * tagged, so without this it would silently keep the portal shut after everything he asked for
	 * was done.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GoblinSiege|Raid|Tuning")
	TSet<FGameplayTag> OptionalTypes;

	// ------------------------------------------------------------------ state

	/** Registration order, which is the order the HUD lists them in. Weak so a destroyed carrier
	 *  goes stale rather than dangling. */
	TArray<TWeakObjectPtr<AGSBurnObjectiveBase>> TrackedCarriers;

	TMap<FGameplayTag, FGSObjectiveTypeBucket> BucketsByType;

	/** Toppleable monuments that satisfy a type when they fall, grouped by that type. Kept apart from
	 *  BucketsByType::Carriers because a monument is not an AGSBurnObjectiveBase and never will be -
	 *  it is not burned, it is pulled over. */
	TMap<FGameplayTag, TArray<TWeakObjectPtr<UGSTopplableComponent>>> MonumentsByType;

	/** Any monument fell. Recounts every monument type rather than trusting the argument, because
	 *  FGSOnToppled carries the TOPPLER, not the monument - so the broadcast cannot say which one it
	 *  came from. Recounting is O(monuments) and happens at most once per statue in a raid. */
	UFUNCTION()
	void HandleMonumentToppled(AActor* Toppler);

	/** Turn a type advance into one line for the player. Kept here rather than in the HUD so the
	 *  wording lives with the rules that produce it. */
	void AnnounceObjective(const FGameplayTag& TypeTag, const FText& DisplayName,
		const FGSObjectiveTypeBucket& Bucket, bool bTypeJustSatisfied);

	/** One entry per DISTINCT type tag placed in the level. */
	TSet<FGameplayTag> RequiredTypes;

	bool bObjectivesComplete = false;

	/**
	 * Not replicated - a UWorldSubsystem cannot replicate. Fine for the single-player slice, where
	 * the HUD runs on the authority. If co-op lands, this moves to AGSGameState (which already
	 * replicates the clock phase) and this field becomes a cached mirror; nothing else changes,
	 * because everyone already asks through GetRaidResult().
	 */
	EGSRaidResult RaidResult = EGSRaidResult::NotEnded;
};
