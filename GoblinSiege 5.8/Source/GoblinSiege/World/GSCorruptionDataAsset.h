// Every number world corruption is tuned by, in one asset a designer owns. Stage 4 of 6, ticket
// #337, 2026-08-27. Ledger rulings 40-45 and 62.
//
// WHY THIS EXISTS, and what belongs here rather than in Config:
// The project's tuning tiers are explicit - structural/lifecycle values and class references live on
// UCLASS(Config = Game) because a subsystem has no CDO; numbers a DESIGNER owns live on a
// UPrimaryDataAsset (precedent: UGSWeaponDataAsset -> Content/Data/Weapons/DA_Weapon_*, and
// UGSRaceDataAsset -> Content/AI/DA_Race_*). So the tick interval and the director class path stay
// in DefaultGame.ini, and everything below - which is every number judged by eye or by feel - moves
// here, where it changes without a rebuild.
//
// ---- THE TWO NUMBERS THAT ARE DELIBERATELY NOT SETTLED ---------------------------------------
// KillSoftKnee and CivilianKillWeight ship at their C++ drafts and are KNOWN to be unfounded. Do not
// "tidy" them by picking rounder values:
//
//   KillSoftKnee = 12 was sized against ruling 19's finite 15-defender pool, BEFORE the 2026-08-23
//   roster ruling made the castle guards Militia with "just a decent amount of them". The real body
//   count is higher, so 12 saturates too early. Counting the roster from .umap files gives reference
//   counts, not instances - this needs the editor, or a watched raid and the kill line in
//   GS.Corruption.Dump.
//
//   CivilianKillWeight = 2.5 has no evidence at all. Ruling 62 decided civilians count MORE than
//   soldiers and DELIBERATELY did not say how much, because that is a feel number. If slaughtering
//   peasants ever becomes the efficient route to a black sky, this is the number that did it.
//
// Both are still readable as cvars (GS.Corruption.*) so they can be dragged mid-playtest; the asset
// is where the answer lands once somebody has watched a full raid.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "GSCorruptionDataAsset.generated.h"

class UCurveFloat;

/**
 * One end of the post-process arc. Michael, 2026-08-25: the world before the burning is "a little
 * more bloom heavy and almost blown out", the corrupted world "a little more gritty".
 *
 * Lives in the DATA header rather than beside the director that applies it, because both the
 * director and this asset need it and neither owns it. Moved here in #337 from GSCorruptionDirector.h.
 *
 * The four colour-grading fields are scalars here and are written into the FVector4 master
 * multiplier (the W) at apply time - FPostProcessSettings stores them as FVector4 where XYZ is
 * per-channel RGB, and treating those as plain floats compiles cleanly and writes garbage.
 */
USTRUCT(BlueprintType)
struct FGSCorruptionGrade
{
	GENERATED_BODY()

	/** Clean 3.0 (hazy glow over everything) -> gritty 0.35 (only hot things glow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float BloomIntensity = 1.f;

	/**
	 * Clean -0.5 (everything blooms) -> gritty 1.2 (only fire blooms).
	 *
	 * THE BEST THING IN THIS STRUCT. Raising the threshold as the world darkens leaves the fires the
	 * PLAYER SET as the only things in frame that glow, so the feature's own fiction does the
	 * lighting work: a clean hamlet glows all over, a corrupted one glows only where it burns. A
	 * tuning pass that flattens this curve loses that for nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float BloomThreshold = 0.f;

	/** Clean +0.75 (overexposed) -> gritty -0.4 (murky). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float AutoExposureBias = 0.f;

	/** Master multiplier only. Clean 1.05 lush -> gritty 0.55 ashen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float Saturation = 1.f;

	/** Clean 0.90 milky -> gritty 1.25 crushed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float Contrast = 1.f;

	/** Clean 1.08 lifted blacks -> gritty 0.92 deep blacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float Gamma = 1.f;

	/** RGB tint. Clean neutral -> gritty warm ash (1.05, 0.92, 0.85). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	FLinearColor Gain = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float FilmGrain = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float Vignette = 0.f;

	/** Chromatic aberration - clean lens 0 -> dirty lens 0.8. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade")
	float SceneFringe = 0.f;
};

UCLASS(BlueprintType)
class GOBLINSIEGE_API UGSCorruptionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- term weights ------------------------------------------------------------------------
	// These MUST sum to 1.0. The subsystem checks and complains loudly rather than renormalising,
	// because a silent correction hides the typo and the symptom - "the world never finishes
	// turning" - is not something anyone traces back to a weight.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeightObjectives = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeightKills = 0.20f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeightStructures = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeightClock = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeightHorde = 0.10f;

	// ---- per-type objective weights ------------------------------------------------------------
	/**
	 * Weight PER TYPE, applied to that type's average completion - not per carrier. The distinction
	 * is the whole of ruling 42 and it has already been got wrong once: weighting per instance let
	 * 67 houses take 75% of the objectives term on L_Tutorial_Island purely by outnumbering
	 * everything, which is GDD 12.1 row 12's complaint one level up. Per type, houses are 6%.
	 *
	 * A tag with no row here falls back to DefaultObjectiveTypeWeight - deliberately not zero, so a
	 * mis-tagged objective is merely cheap rather than invisible.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights")
	TMap<FGameplayTag, float> ObjectiveTypeWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0"))
	float DefaultObjectiveTypeWeight = 0.15f;

	// ---- knees and floors ----------------------------------------------------------------------

	/** Score = N / (N + Knee). 20 smashed props is half the term; nothing ever maxes it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "1.0"))
	float StructureSoftKnee = 20.f;

	/** UNFOUNDED - see the header block. Set this from a watched raid, not from a fresh guess. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "1.0"))
	float KillSoftKnee = 12.f;

	/** UNFOUNDED - see the header block. Ruling 62 says "more", not how much more. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "1.0"))
	float CivilianKillWeight = 2.5f;

	/** The DA_Race_Human archetype row that means civilian. A data key, never a class-name match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights")
	FName CivilianArchetypeRowName = TEXT("Civilian");

	/** Razed, or district-razed, floors the target here whatever the terms say. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Weights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RazedFloor01 = 0.85f;

	// ---- the look ------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Grade")
	FGSCorruptionGrade CleanGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Grade")
	FGSCorruptionGrade GrittyGrade;

	/**
	 * Response curves: corruption 0..1 in, 0..1 out, remapping how fast each output layer reacts.
	 * Null means linear, which is the current shipped behaviour - so leaving these unset changes
	 * nothing and is a valid answer.
	 *
	 * These exist because the arc almost certainly should NOT be linear: the sky wants to hold clean
	 * longer and then fall away, and the grain probably wants to arrive late. That is a judgement to
	 * make in the curve editor while looking at the world, which is exactly the kind of call this
	 * project's rules say to hand over rather than iterate on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Curves")
	TObjectPtr<UCurveFloat> SkyResponse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Curves")
	TObjectPtr<UCurveFloat> FogResponse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Curves")
	TObjectPtr<UCurveFloat> GradeResponse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GoblinSiege|Corruption|Curves")
	TObjectPtr<UCurveFloat> SunResponse;

	/** Weight sum check, so the subsystem can complain at load with the asset's own name. */
	float GetWeightSum() const
	{
		return WeightObjectives + WeightKills + WeightStructures + WeightClock + WeightHorde;
	}

	/**
	 * Curve lookup that treats "no curve" as LINEAR, not as zero.
	 *
	 * Returning zero for an unset curve would mean that authoring this asset and leaving the curves
	 * empty - the obvious first thing anyone does - silently switches every output layer off, and it
	 * would look exactly like the feature being broken rather than like a curve nobody filled in.
	 *
	 * Inline, so this header needs no .cpp: the whole asset is data.
	 */
	static float Evaluate(const UCurveFloat* Curve, float In01);
};

inline float UGSCorruptionDataAsset::Evaluate(const UCurveFloat* Curve, float In01)
{
	const float Clamped = FMath::Clamp(In01, 0.f, 1.f);
	return Curve ? FMath::Clamp(Curve->GetFloatValue(Clamped), 0.f, 1.f) : Clamped;
}
