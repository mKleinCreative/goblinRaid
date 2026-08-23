// Copyright Goblin Siege.

#include "Weapons/GSBowTimingComponent.h"

#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UGSBowTimingComponent::UGSBowTimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Starts DISABLED and is switched on only for the duration of a draw. A bow is held for a few
	// seconds at a time and the pawn already ticks plenty; there is no reason for this to run while
	// the player is walking around.
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Defaulted in code rather than left for a Blueprint to fill in, matching UGSGA_Horn: the
	// component is added to the pawn in C++, so there is no Blueprint pass where anyone would notice
	// three empty slots.
	DrawMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/ScoutV2/Montages/AM_Bow_Draw_Gob.AM_Bow_Draw_Gob")));
	HoldMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/ScoutV2/Montages/AM_Bow_Hold_Gob.AM_Bow_Hold_Gob")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/ScoutV2/Montages/AM_Bow_Release_Gob.AM_Bow_Release_Gob")));
}

void UGSBowTimingComponent::BeginDraw()
{
	// Ignore rather than restart. Enhanced Input can deliver a repeat, and a restart here would
	// silently reset a draw the player was in the middle of judging.
	if (bDrawing)
	{
		return;
	}

	bDrawing = true;
	// Clear last shot's verdict up front: if this draw is cancelled, or the shot it produces is
	// refused, nothing stale is left for a later arrow to pick up.
	LastReleaseQuality = 1.f;
	LastReleaseSpeedScale = 1.f;
	Position01 = 0.f;
	Direction = 1.f;
	Bounces = 0;
	DrawElapsed = 0.f;

	SetComponentTickEnabled(true);

	// Draw now, hold when it finishes. Playing the loop immediately would skip the nock entirely.
	PlayBowMontage(DrawMontage);
	if (const UWorld* World = GetWorld())
	{
		float DrawSeconds = 0.f;
		if (const UAnimMontage* Draw = ActiveBowMontage)
		{
			DrawSeconds = Draw->GetPlayLength();
		}
		if (DrawSeconds > KINDA_SMALL_NUMBER)
		{
			GetWorld()->GetTimerManager().SetTimer(DrawHandoffTimer, this,
				&UGSBowTimingComponent::HandOffToHold, DrawSeconds, false);
		}
	}

	OnBowDrawStarted.Broadcast(TraverseSeconds);
	OnBowDrawProgress.Broadcast(Position01, Bounces);
}

void UGSBowTimingComponent::CancelDraw()
{
	LastReleaseQuality = 1.f;
	LastReleaseSpeedScale = 1.f;
	EndDraw(/*bLoosed=*/false);
}

float UGSBowTimingComponent::ConsumeReleaseQuality()
{
	// Never zero a shot on a stray call. If something releases without a draw in flight - a weapon
	// swapped mid-press, a respawn - the arrow should simply be an ordinary arrow.
	if (!bDrawing)
	{
		return 1.f;
	}

	LastReleaseQuality = GetQualityAt(Position01);
	LastReleaseSpeedScale = GetSpeedScaleAt(Position01);
	EndDraw(/*bLoosed=*/true);
	return LastReleaseQuality;
}

void UGSBowTimingComponent::EndDraw(bool bLoosed)
{
	if (!bDrawing)
	{
		return;
	}

	bDrawing = false;
	SetComponentTickEnabled(false);

	if (const UWorld* World = GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DrawHandoffTimer);
	}

	// A loosed shot plays the recoil; a cancelled draw just stops, because there is no arrow and a
	// recoil without one reads as a shot the player did not take.
	if (bLoosed)
	{
		PlayBowMontage(ReleaseMontage);
	}
	else
	{
		StopBowMontage();
	}

	// Bounces reset here rather than in BeginDraw so that GetSwayOffset reads zero the instant the
	// draw ends. Leaving them set would keep the aim wandering into the next shot.
	Bounces = 0;
	DrawElapsed = 0.f;

	OnBowDrawEnded.Broadcast(bLoosed);
}

void UGSBowTimingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawing)
	{
		return;
	}

	Advance(DeltaTime);
	OnBowDrawProgress.Broadcast(Position01, Bounces);
}

void UGSBowTimingComponent::Advance(float DeltaTime)
{
	DrawElapsed += DeltaTime;

	if (TraverseSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Position01 += Direction * (DeltaTime / TraverseSeconds);

	// PING-PONG, and looped rather than clamped-once. A long frame - a hitch, a breakpoint - can
	// carry the indicator past an end and out the far side; reflecting in a loop keeps it on the bar
	// and keeps the bounce COUNT honest, which matters because bounces are what drive the sway.
	while (Position01 < 0.f || Position01 > 1.f)
	{
		if (Position01 > 1.f)
		{
			Position01 = 2.f - Position01;
			Direction = -1.f;
		}
		else
		{
			Position01 = -Position01;
			Direction = 1.f;
		}
		++Bounces;
	}
}

float UGSBowTimingComponent::GetQualityAt(float InPosition01) const
{
	const float Pos = FMath::Clamp(InPosition01, 0.f, 1.f);
	const float DistanceFromRed = FMath::Abs(Pos - RedCentre);

	// ---- red: flat, and the only band that does not interpolate ---------------------------------
	if (DistanceFromRed <= RedHalfWidth)
	{
		return RedMultiplier;
	}

	// Orange's half-width either side of red. Yellow is whatever remains out to the nearer end of
	// the bar, computed rather than authored so the three bands always tile exactly.
	const float OrangeHalfWidth = FMath::Max(OrangeFraction * 0.5f, KINDA_SMALL_NUMBER);
	const float OrangeOuterEdge = RedHalfWidth + OrangeHalfWidth;

	// ---- orange: outer multiplier at its far edge, inner against red ----------------------------
	if (DistanceFromRed <= OrangeOuterEdge)
	{
		const float T = (OrangeOuterEdge - DistanceFromRed) / OrangeHalfWidth;   // 0 at outer, 1 at red
		return FMath::Lerp(OrangeOuterMultiplier, OrangeInnerMultiplier, FMath::Clamp(T, 0.f, 1.f));
	}

	// ---- yellow: out to whichever end of the bar is on this side --------------------------------
	//
	// Measured against the DISTANCE TO THE NEARER END rather than a fixed width, because red sits at
	// 0.54 rather than dead centre: the yellow run below red is longer than the one above it, and a
	// shared width would make one side's falloff steeper than the other for no reason the player
	// could see.
	const float DistanceToEnd = (Pos < RedCentre) ? RedCentre : (1.f - RedCentre);
	const float YellowWidth = FMath::Max(DistanceToEnd - OrangeOuterEdge, KINDA_SMALL_NUMBER);
	const float TYellow = (YellowWidth - (DistanceFromRed - OrangeOuterEdge)) / YellowWidth;
	return FMath::Lerp(YellowOuterMultiplier, OrangeOuterMultiplier, FMath::Clamp(TYellow, 0.f, 1.f));
}

void UGSBowTimingComponent::PlayBowMontage(const TSoftObjectPtr<UAnimMontage>& Montage)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	UAnimMontage* Loaded = Montage.LoadSynchronous();
	if (!Loaded)
	{
		// Missing montage costs the animation, not the mechanic - the sweep, the damage curve and the
		// arc all run regardless, so this must not return early anywhere upstream.
		return;
	}

	Character->PlayAnimMontage(Loaded);
	ActiveBowMontage = Loaded;
}

void UGSBowTimingComponent::StopBowMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && ActiveBowMontage)
	{
		// Stops THIS montage by name rather than calling StopAnimMontage(nullptr), which stops
		// whatever is playing - a hit reaction or a dodge that started mid-draw must survive.
		Character->StopAnimMontage(ActiveBowMontage);
	}
	ActiveBowMontage = nullptr;
}

void UGSBowTimingComponent::HandOffToHold()
{
	// Guard: the draw may have been cancelled or loosed inside the handoff window, and starting an
	// indefinite loop after that would leave the goblin holding a bow it is no longer drawing.
	if (!bDrawing)
	{
		return;
	}

	PlayBowMontage(HoldMontage);
}

float UGSBowTimingComponent::GetSpeedScaleAt(float InPosition01) const
{
	const float Pos = FMath::Clamp(InPosition01, 0.f, 1.f);
	const float DistanceFromRed = FMath::Abs(Pos - RedCentre);

	// Full draw across the whole red band, not just at its centre. The player is aiming at a band,
	// and an arc that kept bending inside the band they were told is perfect would be lying to them.
	if (DistanceFromRed <= RedHalfWidth)
	{
		return 1.f;
	}

	// Normalised against the distance to the NEARER END, for the same reason GetQualityAt is: red
	// sits at 0.54, so the run below it is longer than the run above, and a shared width would make
	// one side's arc collapse faster than the other for no reason the player could see.
	const float DistanceToEnd = (Pos < RedCentre) ? RedCentre : (1.f - RedCentre);
	const float Span = FMath::Max(DistanceToEnd - RedHalfWidth, KINDA_SMALL_NUMBER);
	const float T = FMath::Clamp((DistanceFromRed - RedHalfWidth) / Span, 0.f, 1.f);

	return FMath::Lerp(1.f, MinSpeedScale, T);
}

float UGSBowTimingComponent::GetCurrentSpeedScale() const
{
	// 1.0 when nothing is being drawn, so the torch arc and any other consumer of the aim component
	// are completely unaffected by this feature existing.
	return bDrawing ? GetSpeedScaleAt(Position01) : 1.f;
}

FRotator UGSBowTimingComponent::GetSwayOffset() const
{
	// Zero bounces is perfectly steady. The first pass across the bar - which includes the red
	// window at 2.7s - is a clean shot; sway is the price of OVERholding, not of drawing.
	if (!bDrawing || Bounces <= 0)
	{
		return FRotator::ZeroRotator;
	}

	const float Amplitude = SwayDegreesPerBounce * static_cast<float>(Bounces);
	const float Phase = DrawElapsed * SwayRateRadPerSec;

	// A figure-eight: pitch runs at twice the yaw's frequency and half its amplitude. Chosen over a
	// circle because a circle spends the whole time moving at one speed, while a lissajous slows at
	// the crossings - which gives the player a readable moment to loose through the target.
	return FRotator(Amplitude * 0.5f * FMath::Sin(Phase * 2.f), Amplitude * FMath::Sin(Phase), 0.f);
}
