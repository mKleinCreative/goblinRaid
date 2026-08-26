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
// Both needed by UHT rather than by the compiler: HandleChannelEnded takes EGSInteractEndReason and
// HandleChannelStarted takes FGameplayTag BY VALUE in a UFUNCTION, so a forward declaration will not
// do - the generated glue has to know their layout.
#include "GameplayTagContainer.h"
#include "Interaction/GSInteractionComponent.h"
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

	/**
	 * The transient line that says what was just achieved. Michael, 2026-08-25: "work on prompts for
	 * when an objective gets completed."
	 *
	 * BlueprintReadOnly as well as BindWidgetOptional, deliberately - see the StaminaBar comment
	 * above. A binding a designer might read from the widget graph and cannot is not a missing
	 * feature, it is a compile error that takes the WHOLE Blueprint down, and this project has
	 * shipped that twice.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<UTextBlock> ObjectivePromptText;

	/** How long an announcement stays on screen. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Prompt", meta = (ClampMin = "0.5"))
	float ObjectivePromptSeconds = 4.f;

	/**
	 * The painted plates behind the objective board and the two prompts.
	 *
	 * These exist ONLY so the art can be shown and hidden with the thing it backs. A plate is a
	 * sibling of its text on the canvas, not a parent, so nothing hides it automatically - left to
	 * itself the announcement banner sits on screen for the whole raid. Optional, like every other
	 * binding here: a HUD without them still plays, it just draws the text bare.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> ObjectiveBoardPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> AnnouncementPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> InteractPromptPlate;

	/**
	 * The board starts CLOSED and opens on M. Michael, having played the first art pass: "the UI is
	 * way too huge, at least the objective marker".
	 *
	 * This is deliberately named for the map it is going to become rather than for the board it is
	 * today - see SetMapOpen. Flip this to true if the board should be up by default.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Map")
	bool bMapOpenByDefault = false;

public:
	/**
	 * Open or close the map panel. Today the panel IS the objective board; M shows and hides it.
	 *
	 * ---- THE SEAM, and nothing more than a seam (ticket #311) -------------------------------
	 * Michael: "we can expand that later to a version of a map, make a stub of that, but don't
	 * implement anything." So this is the one function a map has to hook, and it is named and
	 * placed for that - but there is no map here, no widget for one, and no reader of one. When a
	 * map arrives it becomes another child shown alongside the board inside this call, and the
	 * input binding, the toggle and the open/closed state do not move.
	 *
	 * Named SetMapOpen rather than SetObjectiveBoardOpen because the caller is the M key, and the
	 * key is the thing that will not be renamed later.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|HUD|Map")
	void SetMapOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "GoblinSiege|HUD|Map")
	void ToggleMap();

	UFUNCTION(BlueprintPure, Category = "GoblinSiege|HUD|Map")
	bool IsMapOpen() const { return bMapOpen; }

protected:
	/** Runtime open/closed state; seeded from bMapOpenByDefault at construction. */
	bool bMapOpen = false;

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
	 * BlueprintReadOnly is LOAD-BEARING on these two, unlike every other binding in this class.
	 *
	 * WBP_GSPlayerHUD's own EventGraph read `StaminaBar` and `StaminaText` on Tick - it polled the
	 * character's SprintStamina and wrote them itself, back when the C++ stamina component was not
	 * fed yet. #076 retired that poll, but the hazard it exposed is permanent: a `BindWidgetOptional`
	 * property with no Blueprint visibility is invisible to any graph that reads it, and the compiler
	 * does not warn - it ERRORS:
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

	// ---- the reticle (#148, coloured by #149) --------------------------------------------------
	//
	// Michael: "We need a basic crosshair for this game so people can tell where they're aiming",
	// then "make the reticle turn gold on a valid target".
	//
	// The Image itself and its rune material are authored in WBP_GSPlayerHUD; C++ only tints it. The
	// tint is the whole feature: an always-on reticle answers "where am I pointing", and the colour
	// change answers "will an order actually take" - which was the original complaint.
	//
	// BlueprintReadOnly for the reason the stamina block above sets out at length: any binding a
	// designer's graph might read must be Blueprint-visible or the WHOLE widget fails to compile.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<class UImage> Reticle;

	/** On a valid order target. Matches the weapon wheel's committed-highlight gold, so "gold means
	 *  this will happen" is one language across the whole UI. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Reticle")
	FLinearColor ReticleTargetColour = FLinearColor(1.f, 0.82f, 0.25f, 1.f);

	/** Resting. Dimmer and cooler, so the gold reads as a genuine change rather than a brightness
	 *  wobble - the reticle is on screen permanently and must not nag. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Reticle")
	FLinearColor ReticleIdleColour = FLinearColor(0.85f, 0.87f, 0.95f, 0.5f);

	UFUNCTION()
	void HandleCrosshairTargetChanged(bool bHasTarget, AActor* Target);

	/** Paints the reticle for the current state. Safe to call before the component is found. */
	void RefreshReticle(bool bHasTarget);

	// ---- the interact channel ring (#169) ------------------------------------------------------
	//
	// Michael: "we need to create a visual indicator ... a circle around that reticule we have that
	// slowly fills based on the percentage done with the interaction."
	//
	// UGSInteractionComponent has published OnChannelStarted / OnChannelProgress / OnChannelEnded
	// since it was written and NOTHING has ever bound to them. That is not a missing nicety: a 1.5s
	// hold with no feedback is indistinguishable from a dead key, and it is why the loot chest was
	// reported broken in #163 when it was working correctly the whole time. This class is the first
	// consumer of those delegates.
	//
	// The ring is an Image in WBP_GSPlayerHUD wearing M_GS_ChannelRing, concentric with the Reticle.
	// C++ owns only the fill fraction and the visibility, exactly as it owns only the reticle's tint.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<class UImage> InteractRing;

	// ---- the bow timing bar (#244) ------------------------------------------------------------
	//
	// An Image across the bottom of the canvas wearing M_GS_BowTimingBar: the material owns the
	// gradient (yellow -> orange -> RED -> orange -> yellow) and C++ owns only where the indicator
	// sits and whether the whole thing is on screen. Same division of labour as the channel ring
	// above, and for the same reason - a designer should be able to restyle the bar without a
	// recompile.
	//
	// A UProgressBar was the obvious alternative and cannot express this: a gradient background and
	// an independently-moving indicator are two visuals, and SetPercent only carries one.
	//
	// BlueprintReadOnly is MANDATORY here, not decoration - see the note above on why an unmarked
	// BindWidgetOptional breaks the whole WBP's compile.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<class UImage> BowTimingBar;

	/**
	 * Arrows left (ruling 46). Collapsed on any character with no ammo concept - an AI archer, or a
	 * bow whose data asset names no ArrowItemClass - by the same structural rule the bow timing bar
	 * uses: nothing to ask means nothing to show, rather than a second condition to keep in sync.
	 *
	 * BlueprintReadOnly is MANDATORY here, not house style. #064: a BindWidgetOptional that a
	 * designer graph touches without it fails the WHOLE WBP_GSPlayerHUD compile, and it surfaces as a
	 * red widget in the editor rather than as a C++ error, so it can survive a clean build.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<UTextBlock> ArrowCountText;

	/**
	 * The sliding pointer, as its own Image rather than something the bar's material draws.
	 *
	 * Michael supplied the bar and the pointer as two sprites, so the honest implementation is two
	 * widgets: the bar is a plain texture brush and this is moved across it by a render transform.
	 * That removes the need for M_GS_BowTimingBar entirely. The material path below still works if a
	 * material brush is used instead, so neither approach breaks the other.
	 *
	 * ANCHOR THIS TO THE CENTRE OF THE BAR. The translation applied to it is measured from the bar's
	 * midpoint, so an indicator anchored left will start half a bar-width off.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UImage> BowTimingIndicator;

	/**
	 * Where the coloured fill starts and ends inside T_GS_BowTimingBar, as a fraction of its width.
	 * Measured from the texture: the fill spans x 60..901 of 977, because the wooden frame and the
	 * steel end caps take up the rest. Without this the pointer would run to the very edges of the
	 * image and sit on the caps at either extreme.
	 */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Bow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BowFillUMin = 0.061f;

	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Bow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BowFillUMax = 0.922f;

	/** Vertical nudge for the pointer, so it can straddle the bar rather than sit centred in it. */
	UPROPERTY(EditAnywhere, Category = "GoblinSiege|Bow")
	float BowIndicatorOffsetY = 0.f;

	/** Scalar on the bar material carrying the indicator's 0..1 position. Must match
	 *  M_GS_BowTimingBar. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Bow")
	FName BowIndicatorParameter = FName("IndicatorPos");

	/**
	 * Scalars carrying the band layout to the material, so the gradient stops and the damage bands
	 * cannot drift apart.
	 *
	 * The alternative - authoring the gradient by eye in the material - guarantees that the red the
	 * player aims at eventually stops being the red that pays. The numbers live in
	 * UGSBowTimingComponent and are pushed here on every draw.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Bow")
	FName BowRedCentreParameter = FName("RedCentre");

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Bow")
	FName BowRedHalfWidthParameter = FName("RedHalfWidth");

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Bow")
	FName BowOrangeHalfWidthParameter = FName("OrangeHalfWidth");

	/** Scalar parameter on the ring material that carries 0..1 fill. Must match M_GS_ChannelRing. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Channel")
	FName ChannelPercentParameter = FName("Percent");

	/**
	 * How long a COMPLETED ring stays on screen, full, before it disappears.
	 *
	 * Not decoration, and the mechanism is worth stating exactly because it is easy to get wrong.
	 * UGSInteractionComponent::TickComponent broadcasts a clamped 1.0 and then calls CompleteChannel
	 * on the SAME TICK (GSInteractionComponent.cpp:284-294). So the ring does receive a full value -
	 * it just never gets a frame to draw it in, because OnChannelEnded hides it before the next
	 * present. Without this hold the player watches the ring vanish just shy of closing, every single
	 * time, and reads a success as a failure. That precise illusion is what made the loot chest look
	 * broken in #163, where the debug text died on the same frame for the same reason.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Channel", meta = (ClampMin = "0.0"))
	float ChannelCompleteHoldSeconds = 0.18f;

	UFUNCTION()
	void HandleChannelStarted(class UGSInteractableComponent* Interactable, FGameplayTag VerbTag, float DurationSeconds);

	UFUNCTION()
	void HandleChannelProgress(float Progress);

	UFUNCTION()
	void HandleChannelEnded(bool bCompleted, EGSInteractEndReason Reason);

	/** Push a 0..1 fill into the ring material. No-op when the ring is not authored. */
	void SetChannelProgress(float Progress);

	void ShowChannelRing(bool bVisible);

	/** Bound to UGSBowTimingComponent's delegate triple. See BindToCharacter. */
	UFUNCTION()
	void HandleBowDrawStarted(float TraverseSeconds);

	UFUNCTION()
	void HandleBowDrawProgress(float Position01, int32 Bounces);

	UFUNCTION()
	void HandleBowDrawEnded(bool bLoosed);

	void ShowBowTimingBar(bool bVisible);

	/** The quiver changed (ruling 46, #281). Bound to UACFInventoryComponent::OnInventoryChanged,
	 *  which is parameterless and fires on every mutation including OnRep_Inventory on clients - so
	 *  the count needs no tick and no polling. */
	UFUNCTION()
	void HandleInventoryChanged();

	/** Re-reads the arrow total and repaints, or collapses the text when this character has no ammo
	 *  concept at all. Split out so BindToCharacter can paint immediately without duplicating it. */
	void RefreshArrowCount();

	/** Cached so the fill is not a material lookup per frame. Created lazily from the Image's brush. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> ChannelRingMID;

	/** Cached like ChannelRingMID and for the same reason: GetDynamicMaterial is not free enough to
	 *  call every frame of a draw. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> BowTimingBarMID;

	/** Weak for the same reason BoundCommandComponent is: the pawn routinely outlives this widget. */
	TWeakObjectPtr<class UGSInteractionComponent> BoundInteractionComponent;

	FTimerHandle ChannelHideTimer;

	// ---- the interact prompt (#187) -------------------------------------------------------------
	//
	// The last of UGSInteractionComponent's four delegates to find a consumer, and the other half of
	// what #163 asked for: the ring says how far through you are, this says what you are looking at
	// and what F will do to it. Without it the player has to press the key to discover whether there
	// was anything there at all.
	//
	// Michael's two rulings, 2026-08-18:
	//   1. The prompt does NOT name the key. "Loot the barrel", not "Hold F to loot the barrel" -
	//      the binding has already been remapped once (#058) and baking it into every prompt makes
	//      the next remap a content pass.
	//   2. It DOES show for unavailable interactables, which is why UGSInteractableComponent needed
	//      CanFocus split out of CanInteract. A locked crate reading "Smash it open" teaches the
	//      smash-then-loot chain at the only moment the player cares about it.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")
	TObjectPtr<UTextBlock> InteractPrompt;

	/** Something you can act on right now. Matches the reticle's valid-target gold. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Prompt")
	FLinearColor PromptAvailableColour = FLinearColor(1.f, 0.82f, 0.25f, 1.f);

	// ---- the refusal shake (#187) ---------------------------------------------------------------
	//
	// Michael chose this over a "Locked" caption: "is it possible to make the UI reticule jiggle
	// slightly to give you the indication you can't interact with the item."
	//
	// It is the better answer, and not only because it needs no reading. A locked caption would sit
	// under the reticle every time the player so much as glanced at an unsmashed crate, which is
	// exactly the nagging ReticleIdleColour above was chosen to avoid. A shake costs nothing until
	// the player actually asks a question, and answers it in the same instant.

	/** Peak sideways offset, in slate units. Small on purpose - this is a nudge, not an alarm. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Prompt")
	float RefusalShakePixels = 7.f;

	/** How long the wobble takes to die away. Long enough to read as deliberate, short enough that
	 *  mashing F does not produce a permanently vibrating reticle. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Prompt", meta = (ClampMin = "0.01"))
	float RefusalShakeSeconds = 0.22f;

	/** Oscillations per second. ~14 reads as a shiver rather than a slide. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Prompt")
	float RefusalShakeFrequency = 14.f;

	UFUNCTION()
	void HandleInteractRefused(class UGSInteractableComponent* Interactable);

	/** Seconds remaining in the current shake; 0 when at rest. Ticked in NativeTick. */
	float RefusalShakeRemaining = 0.f;

	/** Advances the shake and writes the reticle's render transform. No-op at rest. */
	void TickRefusalShake(float DeltaSeconds);

	// ---- the grapple haul (#193) ----------------------------------------------------------------
	//
	// A second driver for the SAME ring. Michael's call: hauling a monument over is a hold-and-wait
	// act like looting a crate, so it should look like one rather than inventing a second progress
	// idiom for the same idea.
	//
	// Nothing about the ring is duplicated - these handlers call the identical SetChannelProgress and
	// ShowChannelRing the interaction channel uses. The two sources cannot overlap in practice (you
	// cannot loot a crate while leaning on a rope) and if they ever did, last writer wins, which is
	// the honest behaviour for a single ring.

	UFUNCTION()
	void HandleHaulStarted(AActor* Target, float DurationSeconds);

	UFUNCTION()
	void HandleHaulProgress(float Progress);

	UFUNCTION()
	void HandleHaulEnded(bool bCompleted);

	TWeakObjectPtr<class UGSGrappleHaulComponent> BoundHaulComponent;

	UFUNCTION()
	void HandleFocusChanged(class UGSInteractableComponent* NewFocus);

	/** Paints the prompt for a focus target, or hides it for none. */
	void RefreshPrompt(class UGSInteractableComponent* Focus);

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

	// ------------------------------------------------------------- the icon list (#314)

	/**
	 * The rows with pictures. Optional, like everything else here: leave it out of the .uasset and
	 * the text list carries on alone, which is also what happens on a HUD that has not been
	 * upgraded yet.
	 *
	 * ObjectiveListText and this are NOT alternatives in code - both are filled from the same
	 * BuildDisplayRows pass. Which one the player sees is decided in the .uasset by collapsing the
	 * other, so the two can never disagree about what is required.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UVerticalBox> ObjectiveList;

	/** Set to WBP_GSObjectiveRow. Unset means no icon rows - the text list still works. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Objectives")
	TSubclassOf<class UGSObjectiveRowWidget> ObjectiveRowClass;

	/**
	 * Which picture a burn type draws, keyed by the same tag the director groups on.
	 *
	 * DATA, not a switch statement: a sixth burn type should cost a tag and a row in this map, which
	 * is the argument that made ObjectiveTypeTag a tag rather than an enum in the first place. A tag
	 * with no entry simply draws no type picture and still reads.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Objectives")
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> ObjectiveTypeIcons;

	/** Untouched / in-progress / done, in that order. Indexed by EGSObjectiveRowIcon. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Objectives")
	TSoftObjectPtr<UTexture2D> ObjectiveIconUntouched;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Objectives")
	TSoftObjectPtr<UTexture2D> ObjectiveIconInProgress;

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Objectives")
	TSoftObjectPtr<UTexture2D> ObjectiveIconDone;

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

	/**
	 * The grouping pass: raw carrier rows in, display-ready rows out - collapsed, counted,
	 * pluralised. The ONE place the collapse rule and the required denominator live.
	 */
	void BuildDisplayRows(TArray<FGSObjectiveDisplayRow>& OutRows) const;

	/** Rebuilds ObjectiveList from display rows. No-op without a container and a row class. */
	void RebuildObjectiveRowWidgets(const TArray<FGSObjectiveDisplayRow>& Rows);

	UFUNCTION()
	void HandleObjectiveAnnounced(const FText& Message);

	void HideObjectivePrompt();

	FTimerHandle ObjectivePromptTimer;
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

	/** The order component whose crosshair scan tints the reticle (#149). Weak for the same reason
	 *  every other binding here is: the raid director can destroy and respawn the pawn under us. */
	UPROPERTY()
	TWeakObjectPtr<class UGSHordeCommandComponent> BoundCommandComponent;

	/** Carriers this widget has hooked, so teardown unhooks exactly those and a re-bind does not
	 *  double-subscribe. */
	TArray<TWeakObjectPtr<AGSBurnObjectiveBase>> BoundCarriers;

	/** Stops NativeTick retrying the raid bind once it has succeeded. */
	bool bRaidBound = false;

	/** Last whole second painted - the clock repaints when the digits change, not at frame rate. */
	int32 LastPaintedSecond = -1;
};
