#include "UI/GSPlayerHUDWidget.h"
#include "Characters/GSCharacterBase.h"
#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Raid/GSRaidDirector.h"
#include "Raid/GSScoreSubsystem.h"
#include "Characters/GSStaminaComponent.h"
#include "Horde/GSHordeCommandComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSHUD, Log, All);

void UGSPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BindWidgetOptional means a misspelled widget name in the .uasset fails SILENTLY - the
	// property is simply null and the element never updates. That is the right tradeoff (a rename
	// shouldn't refuse to compile the Blueprint) but it needs a diagnosis, or "the clock doesn't
	// work" is unfalsifiable. One line, once, naming exactly what to add to WBP_GSPlayerHUD.
	auto WarnIfUnbound = [this](const UWidget* Widget, const TCHAR* Name)
	{
		if (!Widget)
		{
			UE_LOG(LogGSHUD, Warning,
				TEXT("[GoblinSiege] HUD widget '%s' is not bound - add a widget with that exact name ")
				TEXT("to WBP_GSPlayerHUD, or that element will stay blank."), Name);
		}
	};

	WarnIfUnbound(HealthBar, TEXT("HealthBar"));
	WarnIfUnbound(HealthText, TEXT("HealthText"));
	WarnIfUnbound(ObjectiveListText, TEXT("ObjectiveListText"));
	WarnIfUnbound(ClockText, TEXT("ClockText"));
	WarnIfUnbound(LivesText, TEXT("LivesText"));
	WarnIfUnbound(AlarmText, TEXT("AlarmText"));

	// EndPanel deliberately NOT passed to WarnIfUnbound. The other six are missing-art warnings at
	// startup; this one only matters at the moment a raid ends, and HandleRaidEnded warns there
	// instead - where the message is actionable rather than one more line in a load log.
	//
	// Hidden here rather than trusting the asset's saved visibility: a designer toggling it visible
	// while editing the panel would otherwise ship a permanent "OUT OF LIVES" over the whole raid.
	if (EndPanel)
	{
		EndPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// GetOwningPlayerPawn can legitimately be null on the first construct if the widget is created
	// before possession; BindToCharacter is public so whoever creates the widget can retry.
	BindToCharacter(Cast<AGSCharacterBase>(GetOwningPlayerPawn()));
	BindToRaid();

	// ---- reticle (#148/#149) -----------------------------------------------------------------
	// Deliberately NOT passed to WarnIfUnbound: a project that has not authored a reticle yet is a
	// valid state, and the six warnings above are for elements that are always supposed to exist.
	//
	// FindComponentByClass on the pawn rather than a cast to AGSPlayerCharacter, matching every other
	// consumer of that component: knowing what is under the crosshair is a property of being able to
	// give orders, not of being the player class.
	if (const APawn* OwnerPawn = GetOwningPlayerPawn())
	{
		BoundCommandComponent = OwnerPawn->FindComponentByClass<UGSHordeCommandComponent>();
	}

	if (UGSHordeCommandComponent* Cmd = BoundCommandComponent.Get())
	{
		Cmd->OnCrosshairTargetChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleCrosshairTargetChanged);
		RefreshReticle(Cmd->HasCrosshairTarget());
	}
	else
	{
		// No component is survivable - the reticle just never lights up. Paint the idle state so it
		// is at least visible and consistent rather than whatever the asset shipped with.
		RefreshReticle(false);
	}
}

void UGSPlayerHUDWidget::NativeDestruct()
{
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnHealthChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);
	}

	// Explicit unbind, for the reason UGSWeaponWheelWidget gives: the pawn outliving this widget is
	// the normal case on a level transition, and a stale dynamic delegate on a destroyed widget is a
	// crash rather than a leak.
	if (UGSHordeCommandComponent* Cmd = BoundCommandComponent.Get())
	{
		Cmd->OnCrosshairTargetChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleCrosshairTargetChanged);
	}
	BoundCommandComponent.Reset();

	UnbindRaid();
	Super::NativeDestruct();
}

void UGSPlayerHUDWidget::HandleCrosshairTargetChanged(bool bHasTarget, AActor* Target)
{
	RefreshReticle(bHasTarget);
}

void UGSPlayerHUDWidget::RefreshReticle(bool bHasTarget)
{
	if (!Reticle)
	{
		return;
	}

	// Tint only - the rune itself, its size and its material are authored in WBP_GSPlayerHUD. Colour
	// is the one thing that has to react to gameplay, so it is the one thing C++ owns.
	Reticle->SetColorAndOpacity(bHasTarget ? ReticleTargetColour : ReticleIdleColour);
}

void UGSPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Retry the raid bind until it takes. The GameState, PlayerState and director can all arrive
	// after this widget is constructed, and on a client they arrive by replication - so there is no
	// single event to wait on that covers all three.
	if (!bRaidBound)
	{
		BindToRaid();
	}

	// The pawn can arrive late too, and a health bar bound to nothing is the original bug.
	if (!BoundCharacter.IsValid())
	{
		if (AGSCharacterBase* Pawn = Cast<AGSCharacterBase>(GetOwningPlayerPawn()))
		{
			BindToCharacter(Pawn);
		}
	}

	RefreshClock();
}

// ====================================================================== health

void UGSPlayerHUDWidget::BindToCharacter(AGSCharacterBase* Character)
{
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnHealthChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);

		// Unbind stamina from the OLD character too. Missing this leaks a binding per respawn, and
		// the raid respawns you up to five times - so the bar would end up driven by a dead pawn's
		// component as well as the live one.
		if (UGSStaminaComponent* OldStam = BoundCharacter->FindComponentByClass<UGSStaminaComponent>())
		{
			OldStam->OnStaminaChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleStaminaChanged);
		}
	}

	BoundCharacter = Character;

	if (!Character)
	{
		return;
	}

	Character->OnHealthChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleHealthChanged);

	// Paint the current value immediately. Without this the bar shows whatever the designer left in
	// the .uasset until the first point of damage, which is exactly the bug this class exists to
	// fix - it just moves the lie from "always" to "until you get hit".
	Refresh(Character->GetHealth(), Character->GetMaxHealth());

	// Stamina, same shape. FindComponentByClass rather than a cast to AGSPlayerCharacter: this
	// widget binds to AGSCharacterBase, and the component being player-only is a fact about who has
	// one, not about who is allowed to display one.
	if (UGSStaminaComponent* Stam = Character->FindComponentByClass<UGSStaminaComponent>())
	{
		Stam->OnStaminaChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleStaminaChanged);
		HandleStaminaChanged(Stam->GetStamina(), Stam->GetMaxStamina());
	}
}

void UGSPlayerHUDWidget::HandleStaminaChanged(float NewStamina, float MaxStamina)
{
	if (StaminaBar)
	{
		StaminaBar->SetPercent(MaxStamina > 0.f ? NewStamina / MaxStamina : 0.f);
	}
	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("STA %d / %d"),
			FMath::RoundToInt(NewStamina), FMath::RoundToInt(MaxStamina))));
	}
}

void UGSPlayerHUDWidget::HandleHealthChanged(float NewHealth, float MaxHealth, float Delta)
{
	Refresh(NewHealth, MaxHealth);

	if (Delta < 0.f)
	{
		OnDamaged(Delta, NewHealth, MaxHealth);
	}
}

void UGSPlayerHUDWidget::Refresh(float NewHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0.f ? FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f) : 0.f);
	}

	if (HealthText)
	{
		// CeilToInt, not RoundToInt: 0.4 health remaining should read as 1, not as 0. A player
		// staring at "HP 0 / 100" while still alive will report it as a bug, and be right to.
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %d / %d"),
			FMath::CeilToInt(FMath::Max(0.f, NewHealth)), FMath::CeilToInt(MaxHealth))));
	}
}

// ====================================================================== raid binding

void UGSPlayerHUDWidget::BindToRaid()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGSGameState* GS = World->GetGameState<AGSGameState>();
	UGSRaidDirector* Director = UGSRaidDirector::Get(this);
	AGSPlayerState* PS = GetOwningPlayerState<AGSPlayerState>();

	// All three or none. A partial bind would leave bRaidBound false and re-run every tick, and
	// re-running a partial bind is how delegates get subscribed twice.
	if (!GS || !Director || !PS)
	{
		return;
	}

	UnbindRaid();

	BoundGameState = GS;
	BoundDirector = Director;
	BoundPlayerState = PS;

	GS->OnRaidClockPhaseChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleRaidClockPhaseChanged);
	GS->OnAlarmPhaseChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleAlarmPhaseChanged);
	PS->OnLivesChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleLivesChanged);
	Director->OnObjectiveRosterChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveRosterChanged);
	Director->OnRaidEnded.AddDynamic(this, &UGSPlayerHUDWidget::HandleRaidEnded);

	bRaidBound = true;

	// Paint everything once, for the same reason the health bar does: an element that is correct
	// only after its first change is an element that is wrong when the player first looks at it.
	HandleObjectiveRosterChanged();
	HandleLivesChanged(PS->GetLives());
	HandleAlarmPhaseChanged(GS->GetAlarmPhase(), GS->GetAlarmPhase());
	RefreshClock();
}

void UGSPlayerHUDWidget::UnbindRaid()
{
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : BoundCarriers)
	{
		if (AGSBurnObjectiveBase* Carrier = Weak.Get())
		{
			Carrier->OnObjectiveListStateChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveListStateChanged);
			Carrier->OnBurnObjectiveProgress.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveProgress);
		}
	}
	BoundCarriers.Reset();

	if (AGSGameState* GS = BoundGameState.Get())
	{
		GS->OnRaidClockPhaseChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleRaidClockPhaseChanged);
		GS->OnAlarmPhaseChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleAlarmPhaseChanged);
	}

	if (AGSPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnLivesChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleLivesChanged);
	}

	if (UGSRaidDirector* Director = BoundDirector.Get())
	{
		Director->OnObjectiveRosterChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveRosterChanged);
		Director->OnRaidEnded.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleRaidEnded);
	}

	BoundGameState.Reset();
	BoundPlayerState.Reset();
	BoundDirector.Reset();
	bRaidBound = false;
}

// ====================================================================== objectives

void UGSPlayerHUDWidget::HandleObjectiveRosterChanged()
{
	UGSRaidDirector* Director = BoundDirector.Get();
	if (!Director)
	{
		return;
	}

	// Re-subscribe to the current roster. Carriers can join mid-raid, so this is not a one-time
	// setup - and dropping the old subscriptions first is what stops a rebuild from stacking them.
	for (const TWeakObjectPtr<AGSBurnObjectiveBase>& Weak : BoundCarriers)
	{
		if (AGSBurnObjectiveBase* Carrier = Weak.Get())
		{
			Carrier->OnObjectiveListStateChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveListStateChanged);
			Carrier->OnBurnObjectiveProgress.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveProgress);
		}
	}
	BoundCarriers.Reset();

	TArray<AGSBurnObjectiveBase*> Carriers;
	Director->GetTrackedCarriers(Carriers);
	for (AGSBurnObjectiveBase* Carrier : Carriers)
	{
		Carrier->OnObjectiveListStateChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveListStateChanged);
		Carrier->OnBurnObjectiveProgress.AddDynamic(this, &UGSPlayerHUDWidget::HandleObjectiveProgress);
		BoundCarriers.Add(Carrier);
	}

	RebuildObjectiveList();
}

void UGSPlayerHUDWidget::HandleObjectiveListStateChanged(EGSObjectiveListState /*NewState*/)
{
	RebuildObjectiveList();
}

void UGSPlayerHUDWidget::HandleObjectiveProgress(float /*Completion01*/)
{
	RebuildObjectiveList();
}

void UGSPlayerHUDWidget::RebuildObjectiveList()
{
	UGSRaidDirector* Director = BoundDirector.Get();
	if (!Director)
	{
		return;
	}

	TArray<FGSObjectiveRow> Rows;
	Director->GetObjectiveRows(Rows);

	OnObjectiveListChanged(Rows);

	if (!ObjectiveListText)
	{
		return;
	}

	// Names only - no arrows, no distances, no waypoints (GDD §2.1, decision 11). The player is
	// told WHAT to burn and finds it themselves; that search is the scouting half of the game.
	//
	// GROUPED BY TYPE (2026-08-05). Eleven houses arrived in the level and named every one of them
	// individually, which made the list longer than the screen and stopped it being read at all.
	// Naming is only useful while the names distinguish things: "The Windmill" tells you where to
	// go, "A House" eleven times tells you nothing. So a type with more carriers than
	// CollapseTypeAbove becomes ONE row with a count, and small types keep their names.
	//
	// Types are kept in first-seen order rather than sorted, so the list does not reshuffle itself
	// as objectives complete - a list that reorders under the player is worse than a long one.
	TArray<FGameplayTag> TypeOrder;
	TMap<FGameplayTag, TArray<const FGSObjectiveRow*>> ByType;
	for (const FGSObjectiveRow& Row : Rows)
	{
		TArray<const FGSObjectiveRow*>& Group = ByType.FindOrAdd(Row.TypeTag);
		if (Group.Num() == 0)
		{
			TypeOrder.Add(Row.TypeTag);
		}
		Group.Add(&Row);
	}

	TArray<FString> Lines;
	Lines.Reserve(TypeOrder.Num() + Rows.Num());

	for (const FGameplayTag& TypeTag : TypeOrder)
	{
		const TArray<const FGSObjectiveRow*>& Group = ByType[TypeTag];

		if (Group.Num() > CollapseTypeAbove)
		{
			int32 Done = 0;
			for (const FGSObjectiveRow* Row : Group)
			{
				if (static_cast<EGSObjectiveListState>(Row->ListState) == EGSObjectiveListState::Complete)
				{
					++Done;
				}
			}

			// The leaf of the tag, pluralised: Objective.Burn.House -> "Houses". Derived rather than
			// authored because a fifth burn type should cost a tag and nothing else - the same
			// argument that made ObjectiveTypeTag a tag instead of an enum.
			FString Leaf = TypeTag.IsValid() ? TypeTag.ToString() : TEXT("Other");
			int32 Dot = INDEX_NONE;
			if (Leaf.FindLastChar(TEXT('.'), Dot))
			{
				Leaf = Leaf.RightChop(Dot + 1);
			}

			Lines.Add(FString::Printf(TEXT("  %s %ss  %d / %d"),
				Done > 0 ? TEXT("[x]") : TEXT("[ ]"), *Leaf, Done, Group.Num()));
			continue;
		}

		for (const FGSObjectiveRow* Row : Group)
		{
			const EGSObjectiveListState State = static_cast<EGSObjectiveListState>(Row->ListState);
			const FString Name = Row->DisplayName.IsEmpty()
				? TEXT("(unnamed objective)")  // an unset ObjectiveDisplayName, visible rather than blank
				: Row->DisplayName.ToString();

			switch (State)
			{
			case EGSObjectiveListState::Complete:
				Lines.Add(FString::Printf(TEXT("  [x] %s"), *Name));
				break;

			case EGSObjectiveListState::Optional:
				// Demoted: a carrier of this type has already burned. Still worth points, no longer
				// required - and the player has to be able to see that, or Q-32's "one of each type"
				// rule is invisible.
				Lines.Add(FString::Printf(TEXT("  [ ] %s  (bonus)"), *Name));
				break;

			case EGSObjectiveListState::Required:
			default:
				if (Row->Completion01 > 0.01f)
				{
					Lines.Add(FString::Printf(TEXT("  [ ] %s  %d%%"), *Name,
						FMath::FloorToInt(Row->Completion01 * 100.f)));
				}
				else
				{
					Lines.Add(FString::Printf(TEXT("  [ ] %s"), *Name));
				}
				break;
			}
		}
	}

	// The header counts TYPES, not carriers - it is the win condition, and the win is one of each
	// type. With eleven houses on the map the two numbers diverge badly, so this has to stay typed.
	const FString Header = FString::Printf(TEXT("BURN  (%d / %d)"),
		Director->GetCompletedTypeCount(), Director->GetRequiredTypeCount());

	// "\n", not LINE_TERMINATOR: on Windows that macro is "\r\n" and Slate renders the carriage
	// return as a missing-glyph box at the end of every line.
	ObjectiveListText->SetText(FText::FromString(
		Lines.Num() > 0
			? Header + TEXT("\n") + FString::Join(Lines, TEXT("\n"))
			: Header));
}

// ====================================================================== clock / lives / alarm

void UGSPlayerHUDWidget::RefreshClock()
{
	AGSGameState* GS = BoundGameState.Get();
	if (!GS || !ClockText)
	{
		return;
	}

	const EGSRaidClockPhase Phase = GS->GetRaidClockPhase();

	// Collapsing counts down the 90s grace instead of the raid - it is a different, more urgent
	// number, and showing 0:00 for ninety seconds would tell the player nothing about how long they
	// have left to reach the portal.
	const bool bCollapsing = (Phase == EGSRaidClockPhase::Collapsing);
	const float Seconds = bCollapsing ? GS->GetCollapseSecondsRemaining() : GS->GetRaidSecondsRemaining();
	const int32 WholeSeconds = FMath::CeilToInt(FMath::Max(0.f, Seconds));

	if (WholeSeconds == LastPaintedSecond)
	{
		return;
	}
	LastPaintedSecond = WholeSeconds;

	if (Phase == EGSRaidClockPhase::NotStarted)
	{
		ClockText->SetText(FText::FromString(TEXT("--:--")));
		return;
	}

	const FString Body = FString::Printf(TEXT("%02d:%02d"), WholeSeconds / 60, WholeSeconds % 60);
	ClockText->SetText(FText::FromString(bCollapsing ? TEXT("COLLAPSING  ") + Body : Body));
}

void UGSPlayerHUDWidget::HandleRaidClockPhaseChanged(EGSRaidClockPhase NewPhase)
{
	LastPaintedSecond = -1; // force a repaint - the format changes with the phase
	RefreshClock();
	OnRaidClockPhaseChanged(NewPhase);
}

void UGSPlayerHUDWidget::HandleAlarmPhaseChanged(EGSAlarmPhase NewPhase, EGSAlarmPhase /*OldPhase*/)
{
	if (AlarmText)
	{
		const TCHAR* Label = TEXT("QUIET");
		switch (NewPhase)
		{
		case EGSAlarmPhase::Suspicious: Label = TEXT("SUSPICIOUS"); break;
		case EGSAlarmPhase::Raid:       Label = TEXT("RAID");       break;
		case EGSAlarmPhase::Razed:      Label = TEXT("RAZED");      break;
		default:                        Label = TEXT("QUIET");      break;
		}
		AlarmText->SetText(FText::FromString(Label));
	}

	OnAlarmPhaseChanged(NewPhase);
}

void UGSPlayerHUDWidget::HandleLivesChanged(int32 LivesRemaining)
{
	if (LivesText)
	{
		LivesText->SetText(FText::FromString(FString::Printf(TEXT("LIVES  %d"), LivesRemaining)));
	}

	OnLivesChanged(LivesRemaining);
}

void UGSPlayerHUDWidget::HandleRaidEnded(EGSRaidResult Result)
{
	// Draw something before delegating. Until #049 this function was only the Blueprint call below,
	// and WBP_GSPlayerHUD does not implement it - so every raid that ended, won or lost, ended in
	// silence.
	FString Title;
	FString Detail;
	switch (Result)
	{
	case EGSRaidResult::Extracted:
		Title  = TEXT("EXTRACTED");
		Detail = TEXT("You made it through the portal.");
		break;
	case EGSRaidResult::LeftBehind:
		Title  = TEXT("LEFT BEHIND");
		Detail = TEXT("The portal collapsed without you.");
		break;
	case EGSRaidResult::OutOfLives:
		Title  = TEXT("OUT OF LIVES");
		Detail = TEXT("The warband is spent.");
		break;
	default:
		// NotEnded reaching here would mean EndRaid was called with it, which it refuses to do.
		Title  = TEXT("RAID OVER");
		Detail = FString();
		break;
	}

	// Objective count on the detail line, so a loss still reports what was achieved. Read live rather
	// than cached, because this fires once and staleness is not a risk.
	FString Score;
	if (const UWorld* World = GetWorld())
	{
		if (const UGSRaidDirector* Director = World->GetSubsystem<UGSRaidDirector>())
		{
			Detail += FString::Printf(TEXT("   %d of %d objective types burned."),
				Director->GetCompletedTypeCount(), Director->GetRequiredTypeCount());
		}

		// The score gets its own line, and asks the subsystem to word itself - the tally owns how it
		// reads, not the widget.
		if (const UGSScoreSubsystem* ScoreSys = World->GetSubsystem<UGSScoreSubsystem>())
		{
			Score = ScoreSys->BuildSummaryLine();
		}
	}

	if (EndScoreText)
	{
		EndScoreText->SetText(FText::FromString(Score));
	}
	else if (!Score.IsEmpty())
	{
		// No dedicated line in the asset: fold it into the detail rather than dropping the score
		// silently. A missing widget should cost layout, never information.
		Detail += TEXT("\n") + Score;
	}

	if (EndTitleText)  { EndTitleText->SetText(FText::FromString(Title)); }
	if (EndDetailText) { EndDetailText->SetText(FText::FromString(Detail)); }
	if (EndPanel)      { EndPanel->SetVisibility(ESlateVisibility::HitTestInvisible); }

	if (!EndPanel)
	{
		// The one case worth a warning: the raid ended and there is nowhere to say so.
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] Raid ended (%s) but the HUD has no `EndPanel` widget, so nothing is "
				 "shown. Add a panel named EndPanel to WBP_GSPlayerHUD."), *Title);
	}

	OnRaidEnded(Result);
}
