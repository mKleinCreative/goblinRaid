// Native parent for WBP_GSPlayerHUD.
//
// The widget already existed and already had a HealthBar and a HealthText - but the health caption
// was the LITERAL string "HP  100 / 100" with no format tokens and no GetHealth call anywhere in
// the package, so it has been decorative since the day it was made. That is why the bar never moved
// during the fire test.
//
// Driving it from C++ rather than from a UMG property binding, for three reasons: the Health
// attribute's change delegate in GAS is non-dynamic and cannot be bound from Blueprint at all, so
// the alternative is polling every frame in the widget graph; BindWidget resolves the two widgets
// by name with no wiring to forget; and a compile error is a better failure than a health bar that
// silently stops updating.
//
// 2026-08-05: extended past health to the four things a raid is actually played by - the objective
// list, the raid clock, lives, and the alarm phase. Before this the player had no way to learn what
// the game wanted of them, which made the tutorial unplayable in the only sense that matters. Every
// element below binds a delegate that ALREADY existed; nothing here polls except the clock, which
// counts a replicated float down and repaints only when the digits change.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Alarm/GSAlarmTypes.h"
#include "Destruction/GSBurnObjectiveBase.h"
#include "Raid/GSRaidTypes.h"
#include "GSPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class AGSCharacterBase;
class AGSGameState;
class AGSPlayerState;
class UGSRaidDirector;

UCLASS()
class GOBLINSIEGE_API UGSPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Point this at a character to follow. Called automatically for the owning pawn; exposed so a
	 *  spectator or a boss bar can retarget it. */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|HUD")
	void BindToCharacter(AGSCharacterBase* Character);

	/**
	 * Bind everything that is not the pawn: the raid director, the game state, the player state.
	 *
	 * Public and safely re-callable, because NativeConstruct can legitimately run before any of the
	 * three exist - a widget built before the GameState has replicated in is the normal case, not an
	 * edge one. NativeTick retries until it takes, then stops trying.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|HUD")
	void BindToRaid();

protected:
	/** BindWidgetOptional, not BindWidget: the names must match what is already in the .uasset, and
	 *  a hard BindWidget would refuse to compile the Blueprint if either were ever renamed - which
	 *  turns a cosmetic problem into a broken HUD.
	 *
	 *  The tradeoff is that a MISSPELLED name fails silently, so NativeConstruct logs every
	 *  unbound widget once - otherwise "the clock doesn't work" has no diagnosis. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	/**
	 * The objective list. GDD §2.1: the HUD NAMES the objectives; finding them stays the player's
	 * job, so there are no arrows and no waypoints here and there never should be (decision 11).
	 *
	 * One text block rather than a scroll box of row widgets: three to five short names do not need
	 * a widget class per row. The structured rows still go to Blueprint via OnObjectiveListChanged,
	 * so a prettier list costs no C++.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveListText;

	/** mm:ss. Switches to the collapse countdown once the portal starts collapsing. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LivesText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AlarmText;

	/**
	 * Stamina, driven from UGSStaminaComponent::OnStaminaChanged - the same delegate shape HealthBar
	 * already uses.
	 *
	 * These widgets already exist in WBP_GSPlayerHUD, but until now the Blueprint POLLED them every
	 * widget tick: cast the owning pawn to BP_GSPlayerCharacter_C, read its SprintStamina float,
	 * divide, SetPercent. Binding them here replaces a per-frame cast with an event, and means the
	 * bar reads the same number the gameplay does rather than a Blueprint's private copy.
	 */
	/**
	 * BlueprintReadOnly is LOAD-BEARING on these two, unlike every other binding in this class.
	 *
	 * WBP_GSPlayerHUD's own EventGraph still reads `StaminaBar` and `StaminaText` on Tick - it polls
	 * the character's SprintStamina and writes them itself, because the C++ stamina component is not
	 * fed yet (the Blueprint migration is still outstanding). A `BindWidgetOptional` property with no
	 * Blueprint visibility is invisible to that graph, and the compiler does not warn - it ERRORS:
	 *   "GSPlayerHUDWidget.StaminaBar is not blueprint visible ... Get StaminaBar"
	 * which fails the WHOLE widget Blueprint, so the Tick poll never runs and the bar freezes at
	 * whatever C++ last wrote. That is exactly how this shipped in #054 and what Michael saw.
	 *
	 * The other bindings here (HealthBar, ClockText, EndPanel...) get away without it only because no
	 * Blueprint graph reads them. Add BlueprintReadOnly to any binding a designer might touch.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<class UProgressBar> StaminaBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<UTextBlock> StaminaText;

	UFUNCTION()
	void HandleStaminaChanged(float NewStamina, float MaxStamina);

	/**
	 * The end-of-raid panel: a container that is hidden for the whole raid and shown once, when it
	 * ends. Add a panel named `EndPanel` to WBP_GSPlayerHUD with `EndTitleText` and `EndDetailText`
	 * inside it.
	 *
	 * This is C++-driven, unlike the four BlueprintImplementableEvents below, and that is a change of
	 * position worth stating. Those four are enrichment hooks over text this class already writes -
	 * an unimplemented OnLivesChanged costs you a nicer lives display, not the lives display. OnRaidEnded
	 * had no such fallback: it was the ONLY output of the raid loop with nothing behind it, so a
	 * finished raid produced one log line and no screen at all (#049). A game needs to be able to tell
	 * you that you lost without a designer having implemented an event first.
	 *
	 * The Blueprint event still fires afterwards, so a real end screen can replace this entirely.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EndPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EndTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EndDetailText;

	/** The score line. Optional like the rest - if it is absent the score is appended to
	 *  EndDetailText instead, because a missing widget should cost layout, not information. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EndScoreText;

	/**
	 * A burn type with MORE carriers than this collapses to a single counted row.
	 *
	 * 2 by default, which keeps the tutorial's two wheat fields named individually ("The Wheat
	 * Field", "The Farm Field" - both worth finding) while the eleven houses become one "Houses
	 * 0 / 11". Naming is only worth the space while the names distinguish things; "A House"
	 * repeated eleven times is noise that pushes the objectives that DO have names off the screen.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD", meta = (ClampMin = "1"))
	int32 CollapseTypeAbove = 2;

	// ------------------------------------------------------------------ Blueprint hooks

	/** Called on damage so a Blueprint can flash the bar, shake, play a sound - anything that wants
	 *  to react to a hit without duplicating the binding. Delta is negative. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnDamaged(float Delta, float NewHealth, float MaxHealth);

	/** Structured objective rows, for a Blueprint that wants a real list. Fires alongside the text
	 *  rebuild, not instead of it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnObjectiveListChanged(const TArray<FGSObjectiveRow>& Rows);

	/** The raid is over, and Result says how. The end panel lives here - what a lose screen looks
	 *  like is not a C++ decision. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnRaidEnded(EGSRaidResult Result);

	/** Restyle cue for the timer (impatient at FinalWarning, alarming at Collapsing) without the
	 *  widget polling for a threshold crossing. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnRaidClockPhaseChanged(EGSRaidClockPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnAlarmPhaseChanged(EGSAlarmPhase NewPhase);

	/** For pip widgets, which C++ has no business building. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GoblinSiege|HUD")
	void OnLivesChanged(int32 LivesRemaining);

	// ------------------------------------------------------------------ delegate handlers

	/** Bound to AGSCharacterBase::OnHealthChanged. */
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth, float Delta);

	UFUNCTION()
	void HandleObjectiveRosterChanged();

	/**
	 * Bound to EVERY tracked carrier's own list-state delegate.
	 *
	 * This is what lets a client see a demotion at all: the director's Q-37 pass runs server-side,
	 * but ListState is a replicated property with its own OnRep, so the carrier tells the HUD
	 * directly on whichever machine the HUD is running.
	 */
	UFUNCTION()
	void HandleObjectiveListStateChanged(EGSObjectiveListState NewState);

	UFUNCTION()
	void HandleObjectiveProgress(float Completion01);

	UFUNCTION()
	void HandleRaidClockPhaseChanged(EGSRaidClockPhase NewPhase);

	UFUNCTION()
	void HandleAlarmPhaseChanged(EGSAlarmPhase NewPhase, EGSAlarmPhase OldPhase);

	UFUNCTION()
	void HandleLivesChanged(int32 LivesRemaining);

	UFUNCTION()
	void HandleRaidEnded(EGSRaidResult Result);

	// ------------------------------------------------------------------ internals

	void Refresh(float NewHealth, float MaxHealth);
	void RebuildObjectiveList();
	void RefreshClock();
	void UnbindRaid();

private:
	UPROPERTY()
	TWeakObjectPtr<AGSCharacterBase> BoundCharacter;

	UPROPERTY()
	TWeakObjectPtr<AGSGameState> BoundGameState;

	UPROPERTY()
	TWeakObjectPtr<AGSPlayerState> BoundPlayerState;

	UPROPERTY()
	TWeakObjectPtr<UGSRaidDirector> BoundDirector;

	/** Carriers this widget has hooked, so teardown unhooks exactly those and a re-bind does not
	 *  double-subscribe. */
	TArray<TWeakObjectPtr<AGSBurnObjectiveBase>> BoundCarriers;

	/** Stops NativeTick retrying the raid bind once it has succeeded. */
	bool bRaidBound = false;

	/** Last whole second painted - the clock repaints when the digits change, not at frame rate. */
	int32 LastPaintedSecond = -1;
};
