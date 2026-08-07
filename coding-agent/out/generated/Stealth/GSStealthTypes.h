// Stealth detection data types - stance, detection tuning, watchman doze tuning and the
// per-target sighting record (design doc §2.4 "Stealth - the quiet half of the raid": a guard's
// sighting must hold ~1.5s to confirm, crouch and cover tighten effective detection range).
// 2026-08-04 (Block D): first stealth types in the module - nothing detection-side existed
// before, so every number here is tuning-owned and lives on the observer component.
#pragma once

#include "CoreMinimal.h"
#include "GSStealthTypes.generated.h"

/**
 * How a would-be-seen actor is carrying itself. Crouch is the Scout's stealth stance (§2.4.2):
 * it tightens every guard's effective detection range. Hidden is the "never generates a sighting"
 * escape hatch used for downed/dead/teleporting actors so they cannot spuriously confirm.
 */
UENUM(BlueprintType)
enum class EGSStealthStance : uint8
{
	Standing = 0,
	Crouched,
	Hidden
};

/**
 * Per-observer detection tuning. One readable rule (§2.4): range scales with stance and cover,
 * and a sighting must hold ConfirmHoldSeconds to become a signal. Everything else is trim.
 */
USTRUCT(BlueprintType)
struct GOBLINSIEGE_API FGSDetectionTuning
{
	GENERATED_BODY()

	/** Range at which a standing, fully exposed goblin can be seen at all, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0"))
	float BaseSightRange = 2400.f;

	/** Multiplier applied to BaseSightRange while the target is crouched (§2.4.2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchRangeMultiplier = 0.55f;

	/** Multiplier applied when only a sliver of the target clears the cover geometry. Exposure of
	 *  1.0 lerps this back to 1.0 - hedgerows, fences and haycarts are the hamlet's cover language. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FullCoverRangeMultiplier = 0.3f;

	/** Half-angle of the guard's vision cone, degrees from the (possibly drooping) view direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float PeripheralHalfAngleDegrees = 60.f;

	/** THE rule: hold the sighting this long to confirm. Break line of sight first and it cost
	 *  the player nothing - the timer resets to zero, it is not a partial signal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.05"))
	float ConfirmHoldSeconds = 1.5f;

	/** Unbroken seconds out of sight after which a confirmed target is forgotten and could be
	 *  confirmed (and re-signalled) again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0"))
	float ForgetSeconds = 4.f;

	/** Fraction of body sample points that must clear cover before the guard sees anything at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExposureToSee = 0.05f;

	float GetStanceRangeMultiplier(EGSStealthStance Stance) const
	{
		switch (Stance)
		{
		case EGSStealthStance::Crouched:	return FMath::Max(0.f, CrouchRangeMultiplier);
		case EGSStealthStance::Hidden:		return 0.f;
		default:							return 1.f;
		}
	}

	/** Stance x cover x doze, folded into the one number the range test uses. */
	float GetEffectiveRange(EGSStealthStance Stance, float Exposure01, float DozeRangeMultiplier) const
	{
		const float CoverMultiplier = FMath::Lerp(FMath::Clamp(FullCoverRangeMultiplier, 0.f, 1.f), 1.f, FMath::Clamp(Exposure01, 0.f, 1.f));
		return FMath::Max(0.f, BaseSightRange) * GetStanceRangeMultiplier(Stance) * CoverMultiplier * FMath::Max(0.f, DozeRangeMultiplier);
	}
};

/**
 * The watchman's doze (§2.4): a guard who has seen nothing for a while lets his vision cone sag
 * toward his boots. Purely an observer-side modifier - it shortens range, narrows the cone and
 * pitches the look direction down, and any sighting or noise snaps it back to zero.
 */
USTRUCT(BlueprintType)
struct GOBLINSIEGE_API FGSDozeTuning
{
	GENERATED_BODY()

	/** Watchmen doze; roving patrols can turn it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze")
	bool bCanDoze = true;

	/** Quiet seconds before the droop starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze", meta = (ClampMin = "0.0"))
	float DrowseStartSeconds = 10.f;

	/** Quiet seconds at which the droop is at its worst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze", meta = (ClampMin = "0.1"))
	float FullDozeSeconds = 26.f;

	/** How far down the cone pitches at full doze, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze", meta = (ClampMin = "0.0", ClampMax = "85.0"))
	float MaxDroopDegrees = 38.f;

	/** Range multiplier at full doze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DozeRangeMultiplier = 0.45f;

	/** Cone half-angle multiplier at full doze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doze", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DozeConeMultiplier = 0.6f;

	float GetDozeAlpha(float DozeSeconds) const
	{
		if (!bCanDoze)
		{
			return 0.f;
		}
		const float Span = FMath::Max(0.01f, FullDozeSeconds - DrowseStartSeconds);
		return FMath::Clamp((DozeSeconds - DrowseStartSeconds) / Span, 0.f, 1.f);
	}
};

/** One observer's running book on one target. Lives only on the server. */
USTRUCT(BlueprintType)
struct GOBLINSIEGE_API FGSSightingRecord
{
	GENERATED_BODY()

	/** Seconds of unbroken sight accumulated toward ConfirmHoldSeconds. Reset to 0 the instant
	 *  line of sight breaks - duck behind the haycart and you were never there. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	float ConfirmTimer = 0.f;

	/** Seconds since this observer last had the target in sight. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	float TimeSinceLastSeen = 0.f;

	/** True once the hold landed. Latches so a single confirm emits a single soft signal. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	bool bConfirmed = false;

	/** True if the target cleared cover, range and cone on the most recent evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	bool bVisibleNow = false;

	/** Fraction of body sample points that cleared cover geometry last evaluation. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	float LastExposure01 = 0.f;

	/** Where the target was when last seen - the investigate point for the noise/search pass. */
	UPROPERTY(BlueprintReadOnly, Category = "Sighting")
	FVector LastKnownLocation = FVector::ZeroVector;
};
