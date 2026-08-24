#include "UI/GSPlayerHUDWidget.h"
#include "Characters/GSCharacterBase.h"
#include "Core/GSGameState.h"
#include "Core/GSPlayerState.h"
#include "Raid/GSRaidDirector.h"
#include "Raid/GSScoreSubsystem.h"
#include "Characters/GSStaminaComponent.h"
#include "Weapons/GSBowTimingComponent.h"
#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Components/ACFInventoryComponent.h"
#include "Items/ACFItem.h"
#include "Horde/GSHordeCommandComponent.h"
#include "Interaction/GSInteractionComponent.h"
#include "Interaction/GSInteractableComponent.h"
#include "Weapons/GSGrappleHaulComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
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

	// ---- interact channel ring (#169) ---------------------------------------------------------
	// Same shape as the reticle above, and not warned about for the same reason: a project with no
	// interactables authored yet is a valid state.
	if (const APawn* OwnerPawn = GetOwningPlayerPawn())
	{
		BoundInteractionComponent = OwnerPawn->FindComponentByClass<UGSInteractionComponent>();
	}

	if (UGSInteractionComponent* Interaction = BoundInteractionComponent.Get())
	{
		Interaction->OnChannelStarted.AddDynamic(this, &UGSPlayerHUDWidget::HandleChannelStarted);
		Interaction->OnChannelProgress.AddDynamic(this, &UGSPlayerHUDWidget::HandleChannelProgress);
		Interaction->OnChannelEnded.AddDynamic(this, &UGSPlayerHUDWidget::HandleChannelEnded);
		Interaction->OnFocusChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleFocusChanged);
		Interaction->OnInteractRefused.AddDynamic(this, &UGSPlayerHUDWidget::HandleInteractRefused);
	}
	else if (InteractRing)
	{
		// The ring is authored but nothing will ever drive it. Worth a line: a permanently invisible
		// ring is the same symptom as a broken one, and this distinguishes them.
		UE_LOG(LogGSHUD, Warning,
			TEXT("[GoblinSiege] HUD has an InteractRing but the pawn has no UGSInteractionComponent - ")
			TEXT("the channel ring will never appear."));
	}

	// ---- grapple haul (#193) ------------------------------------------------------------------
	// Optional in the strongest sense: the component is added on BP_GSPlayerCharacter, not in native
	// code, so a pawn without it is normal rather than broken. No warning for the same reason the
	// reticle gets none.
	if (const APawn* OwnerPawn = GetOwningPlayerPawn())
	{
		BoundHaulComponent = OwnerPawn->FindComponentByClass<UGSGrappleHaulComponent>();
	}

	if (UGSGrappleHaulComponent* Haul = BoundHaulComponent.Get())
	{
		Haul->OnHaulStarted.AddDynamic(this, &UGSPlayerHUDWidget::HandleHaulStarted);
		Haul->OnHaulProgress.AddDynamic(this, &UGSPlayerHUDWidget::HandleHaulProgress);
		Haul->OnHaulEnded.AddDynamic(this, &UGSPlayerHUDWidget::HandleHaulEnded);
	}

	// Start hidden regardless of what the asset saved, for the reason EndPanel is hidden above: a
	// designer leaving it visible mid-edit would otherwise ship a ring stuck on screen all raid.
	ShowChannelRing(false);
	RefreshPrompt(nullptr);
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

	if (UGSInteractionComponent* Interaction = BoundInteractionComponent.Get())
	{
		Interaction->OnChannelStarted.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleChannelStarted);
		Interaction->OnChannelProgress.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleChannelProgress);
		Interaction->OnChannelEnded.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleChannelEnded);
		Interaction->OnFocusChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleFocusChanged);
		Interaction->OnInteractRefused.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleInteractRefused);
	}
	BoundInteractionComponent.Reset();

	// The bow timing component is found off the character rather than cached in its own weak
	// pointer, because unlike the interaction component it never moves between actors - it is on the
	// player pawn or it does not exist. Unbinding through the old character is enough.
	if (const AGSCharacterBase* Old = BoundCharacter.Get())
	{
		if (UGSBowTimingComponent* Bow = Old->FindComponentByClass<UGSBowTimingComponent>())
		{
			Bow->OnBowDrawStarted.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawStarted);
			Bow->OnBowDrawProgress.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawProgress);
			Bow->OnBowDrawEnded.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawEnded);
		}
	}

	if (UGSGrappleHaulComponent* Haul = BoundHaulComponent.Get())
	{
		Haul->OnHaulStarted.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHaulStarted);
		Haul->OnHaulProgress.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHaulProgress);
		Haul->OnHaulEnded.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleHaulEnded);
	}
	BoundHaulComponent.Reset();

	// A pending hide would fire into a destroyed widget.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChannelHideTimer);
	}

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

// ---- interact channel ring (#169) -------------------------------------------------------------

void UGSPlayerHUDWidget::HandleChannelStarted(UGSInteractableComponent* Interactable,
	FGameplayTag VerbTag, float DurationSeconds)
{
	// A previous completion's hold could still be pending; a new channel supersedes it.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChannelHideTimer);
	}

	// Zero BEFORE showing. Otherwise the ring appears carrying the previous channel's fill for one
	// frame, which reads as the hold having started already part-done.
	SetChannelProgress(0.f);
	ShowChannelRing(true);
}

void UGSPlayerHUDWidget::HandleChannelProgress(float Progress)
{
	SetChannelProgress(Progress);
}

void UGSPlayerHUDWidget::HandleChannelEnded(bool bCompleted, EGSInteractEndReason Reason)
{
	if (!bCompleted)
	{
		// Released or interrupted: the ring vanishing IS the feedback that it did not take.
		ShowChannelRing(false);
		return;
	}

	// Completed. Snap to a genuinely full circle and hold it briefly - see ChannelCompleteHoldSeconds
	// for why this is not decoration.
	SetChannelProgress(1.f);

	UWorld* World = GetWorld();
	if (!World || ChannelCompleteHoldSeconds <= 0.f)
	{
		ShowChannelRing(false);
		return;
	}

	World->GetTimerManager().SetTimer(ChannelHideTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]() { ShowChannelRing(false); }),
		ChannelCompleteHoldSeconds, false);
}

void UGSPlayerHUDWidget::SetChannelProgress(float Progress)
{
	if (!InteractRing)
	{
		return;
	}

	// Lazily, and cached: GetDynamicMaterial creates the MID on first call and returns the same one
	// after, but it is not free enough to want per-frame during a channel.
	if (!ChannelRingMID)
	{
		ChannelRingMID = InteractRing->GetDynamicMaterial();

		if (!ChannelRingMID)
		{
			// The Image has a plain texture brush, not a material. The ring cannot fill, and silently
			// showing an unmoving circle is worse than saying so.
			UE_LOG(LogGSHUD, Warning,
				TEXT("[GoblinSiege] InteractRing has no material brush, so it cannot show progress - ")
				TEXT("set its brush to M_GS_ChannelRing (or another material with a '%s' scalar)."),
				*ChannelPercentParameter.ToString());
			return;
		}
	}

	ChannelRingMID->SetScalarParameterValue(ChannelPercentParameter, FMath::Clamp(Progress, 0.f, 1.f));
}

void UGSPlayerHUDWidget::HandleFocusChanged(UGSInteractableComponent* NewFocus)
{
	RefreshPrompt(NewFocus);
}

void UGSPlayerHUDWidget::RefreshPrompt(UGSInteractableComponent* Focus)
{
	if (!InteractPrompt)
	{
		return;
	}

	// Available only. A locked container deliberately shows NOTHING here - it answers through the
	// refusal shake instead, at the moment the player presses rather than every time they look.
	if (!IsValid(Focus) || !Focus->IsAvailable())
	{
		InteractPrompt->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	InteractPrompt->SetText(Focus->GetPromptText());
	InteractPrompt->SetColorAndOpacity(FSlateColor(PromptAvailableColour));
	InteractPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGSPlayerHUDWidget::HandleHaulStarted(AActor* Target, float DurationSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChannelHideTimer);
	}

	SetChannelProgress(0.f);
	ShowChannelRing(true);
}

void UGSPlayerHUDWidget::HandleHaulProgress(float Progress)
{
	SetChannelProgress(Progress);
}

void UGSPlayerHUDWidget::HandleHaulEnded(bool bCompleted)
{
	// Same reasoning as the interaction channel: on success hold a full ring briefly, because the
	// statue starts falling on the same frame the haul completes and the ring would otherwise vanish
	// at 99% during the one moment the player is looking at something else.
	HandleChannelEnded(bCompleted, EGSInteractEndReason::Completed);
}

void UGSPlayerHUDWidget::TickRefusalShake(float DeltaSeconds)
{
	if (RefusalShakeRemaining <= 0.f || !Reticle)
	{
		return;
	}

	RefusalShakeRemaining = FMath::Max(0.f, RefusalShakeRemaining - DeltaSeconds);

	// Damped horizontal oscillation: amplitude falls off with the time remaining, so it settles
	// instead of stopping dead mid-swing. Sideways only - a vertical shake on a centre-screen reticle
	// reads as the camera moving rather than the UI answering you.
	const float Alpha = RefusalShakeRemaining / FMath::Max(RefusalShakeSeconds, 0.01f);
	const float Elapsed = FMath::Max(RefusalShakeSeconds, 0.01f) - RefusalShakeRemaining;
	const float Offset = FMath::Sin(Elapsed * RefusalShakeFrequency * 2.f * PI) * RefusalShakePixels * Alpha;

	FWidgetTransform Transform = Reticle->GetRenderTransform();
	Transform.Translation.X = Offset;
	Reticle->SetRenderTransform(Transform);

	// Land exactly on zero. Leaving a sub-pixel offset behind would nudge the reticle permanently
	// off-centre after enough refusals, and the reticle is the thing the player aims with.
	if (RefusalShakeRemaining <= 0.f)
	{
		Transform.Translation.X = 0.f;
		Reticle->SetRenderTransform(Transform);
	}
}

void UGSPlayerHUDWidget::HandleInteractRefused(UGSInteractableComponent* Interactable)
{
	// Restart rather than accumulate. Mashing F should re-shake from full amplitude, not stack into a
	// longer and longer vibration.
	RefusalShakeRemaining = FMath::Max(RefusalShakeSeconds, 0.01f);
}

void UGSPlayerHUDWidget::ShowChannelRing(bool bVisible)
{
	if (!InteractRing)
	{
		return;
	}

	// Collapsed rather than Hidden: the ring is in a canvas panel at a fixed size, so it costs no
	// layout either way, and Collapsed is what the rest of this class uses for "not now".
	InteractRing->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UGSPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TickRefusalShake(InDeltaTime);

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

		// The quiver, same reason. Five respawns without this and five dead pawns' inventories are
		// all writing to one text block.
		if (UACFInventoryComponent* OldInv = BoundCharacter->FindComponentByClass<UACFInventoryComponent>())
		{
			OldInv->OnInventoryChanged.RemoveDynamic(this, &UGSPlayerHUDWidget::HandleInventoryChanged);
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

	// The bow timing bar, same shape again. Only the player pawn carries a timing component, so on
	// any other character this simply finds nothing and the bar never appears - which is the same
	// structural guarantee the gameplay side relies on, rather than a second check to keep in sync.
	if (UGSBowTimingComponent* Bow = Character->FindComponentByClass<UGSBowTimingComponent>())
	{
		Bow->OnBowDrawStarted.AddDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawStarted);
		Bow->OnBowDrawProgress.AddDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawProgress);
		Bow->OnBowDrawEnded.AddDynamic(this, &UGSPlayerHUDWidget::HandleBowDrawEnded);
	}

	// The arrow count, same shape a fourth time. OnInventoryChanged rather than OnItemAdded /
	// OnItemRemoved: it is parameterless, fires on every mutation including OnRep_Inventory on
	// clients, and we re-read the total anyway - so the delta the other two carry is not wanted.
	if (UACFInventoryComponent* Inv = Character->FindComponentByClass<UACFInventoryComponent>())
	{
		Inv->OnInventoryChanged.AddDynamic(this, &UGSPlayerHUDWidget::HandleInventoryChanged);
	}

	// Paint immediately, and OUTSIDE the block above so a character with no inventory still gets the
	// text collapsed rather than inheriting the previous pawn's number.
	RefreshArrowCount();

	// Hidden until a draw starts, whatever the asset was saved with. Deliberately outside the block
	// above: a character with no timing component must also get a hidden bar, or swapping to one
	// mid-draw would leave the previous pawn's bar on screen.
	ShowBowTimingBar(false);
}

void UGSPlayerHUDWidget::HandleInventoryChanged()
{
	RefreshArrowCount();
}

void UGSPlayerHUDWidget::RefreshArrowCount()
{
	if (!ArrowCountText)
	{
		return;
	}

	// Which item counts as "arrows" is a property of the equipped bow, exactly as the gameplay side
	// reads it in UGSGA_BowShot::GetAmmoItemClass. Asking the same place means the HUD cannot show a
	// number the bow does not spend.
	const AGSCharacterBase* Character = BoundCharacter.Get();
	const UGSWeaponComponent* WeaponComp =
		Character ? Character->FindComponentByClass<UGSWeaponComponent>() : nullptr;
	const UGSWeaponDataAsset* Weapon = WeaponComp ? WeaponComp->GetEquippedWeapon() : nullptr;
	const UACFInventoryComponent* Inventory =
		Character ? Character->FindComponentByClass<UACFInventoryComponent>() : nullptr;

	// No ammo concept on this character - an AI archer, or a bow that names no ArrowItemClass.
	// Collapse rather than show a zero: an unlimited bow reading "0" is worse than no readout at all.
	if (!Weapon || !Weapon->ArrowItemClass || !Inventory)
	{
		ArrowCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ArrowCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	ArrowCountText->SetText(FText::AsNumber(
		Inventory->GetTotalCountOfItemsByClass(Weapon->ArrowItemClass)));
}

void UGSPlayerHUDWidget::HandleBowDrawStarted(float TraverseSeconds)
{
	ShowBowTimingBar(true);
	HandleBowDrawProgress(0.f, 0);
}

void UGSPlayerHUDWidget::HandleBowDrawProgress(float Position01, int32 Bounces)
{
	if (!BowTimingBar)
	{
		return;
	}

	const float Pos = FMath::Clamp(Position01, 0.f, 1.f);

	// ---- THE POINTER SPRITE, IF THERE IS ONE ----------------------------------------------------
	// Michael's art is a bar and a separate pointer, so the pointer is a widget that slides rather
	// than something a material draws. Measured from the BAR's live width, not from a stored number,
	// so the pointer stays aligned at any HUD scale or resolution.
	if (BowTimingIndicator)
	{
		const float BarWidth = BowTimingBar->GetCachedGeometry().GetLocalSize().X;
		if (BarWidth > KINDA_SMALL_NUMBER)
		{
			// Fill fraction, not 0..1 across the whole image: the wooden frame and steel caps own
			// the outer ~14% and the pointer must not wander onto them.
			const float U = FMath::Lerp(BowFillUMin, BowFillUMax, Pos);
			BowTimingIndicator->SetRenderTranslation(
				FVector2D((U - 0.5f) * BarWidth, BowIndicatorOffsetY));
		}
	}

	// ---- THE MATERIAL PATH, still supported ------------------------------------------------------
	// Only taken when the bar's brush is a material. With the two-sprite setup above there is no
	// material and GetDynamicMaterial returns null every frame, so this must not log in that case -
	// which is why the warning now depends on there being no indicator widget either.
	if (!BowTimingBarMID)
	{
		BowTimingBarMID = BowTimingBar->GetDynamicMaterial();

		if (!BowTimingBarMID)
		{
			// A plain texture brush AND no pointer widget means nothing on screen can move, which
			// reads as a broken mechanic rather than as missing setup. With a pointer widget present
			// this is the normal, intended configuration and must stay silent.
			if (!BowTimingIndicator)
			{
				UE_LOG(LogGSHUD, Warning,
					TEXT("[GoblinSiege] BowTimingBar has no material brush and there is no ")
					TEXT("BowTimingIndicator widget, so nothing can move. Add an Image named ")
					TEXT("BowTimingIndicator, or give the bar a material with a '%s' scalar."),
					*BowIndicatorParameter.ToString());
			}
			return;
		}

		// Push the band layout ONCE per draw, when the MID is first made. The material draws the
		// gradient from these, so the red the player aims at is the same red
		// UGSBowTimingComponent pays out on - they cannot drift apart by being authored twice.
		if (const AGSCharacterBase* Character = BoundCharacter.Get())
		{
			if (const UGSBowTimingComponent* Bow =
					Character->FindComponentByClass<UGSBowTimingComponent>())
			{
				BowTimingBarMID->SetScalarParameterValue(BowRedCentreParameter, Bow->GetRedCentre());
				BowTimingBarMID->SetScalarParameterValue(BowRedHalfWidthParameter, Bow->GetRedHalfWidth());
				BowTimingBarMID->SetScalarParameterValue(BowOrangeHalfWidthParameter, Bow->GetOrangeHalfWidth());
			}
		}
	}

	BowTimingBarMID->SetScalarParameterValue(BowIndicatorParameter, Pos);
}

void UGSPlayerHUDWidget::HandleBowDrawEnded(bool bLoosed)
{
	ShowBowTimingBar(false);
}

void UGSPlayerHUDWidget::ShowBowTimingBar(bool bVisible)
{
	if (!BowTimingBar)
	{
		return;
	}

	const ESlateVisibility Vis =
		bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	BowTimingBar->SetVisibility(Vis);
	if (BowTimingIndicator)
	{
		BowTimingIndicator->SetVisibility(Vis);
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
