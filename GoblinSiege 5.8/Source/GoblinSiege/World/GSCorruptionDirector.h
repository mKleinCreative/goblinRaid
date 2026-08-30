// The output stage of world corruption: one scalar in, sky / fog / sun / post-process out.
// Written 2026-08-25, ticket #309. Ledger ruling 40, and the grade arc Michael set on 2026-08-25:
// "the good world before the burning is a little more bloom heavy and almost blown out, then the
// darkness infested world a little more gritty".
//
// ---- WHY THIS IS AN ACTOR AND NOT MORE SUBSYSTEM --------------------------------------------
// Discovery is a one-off at BeginPlay; ownership of actor pointers across respawns and streaming is
// not something a subsystem should be doing per tick. The subsystem finds one of these or spawns it,
// so a level artist can place a BP subclass to override the look per map WITHOUT the feature
// depending on anyone remembering to. That dependency is exactly what makes the horn appear broken
// when its arrival markers are missing.
//
// ---- THE GRADE IS TWO-ENDED AND LIVE AT ALL TIMES -------------------------------------------
// The clean world is an EFFECT, not the absence of one, so this volume cannot fade in from
// transparent - BlendWeight stays at 1 and every field lerps between a Clean and a Gritty value.
// Consequence, stated because it is a real cost: at priority 1000 and unbound, ours takes over from
// a level artist's grade. Accepted because the maps that matter have none - L_CombatArena,
// L_CombatArena_Hills, GS_BurnTest and L_Hamlet_01/02/03 contain no APostProcessVolume at all. Only
// L_Hamlet_T1, L_Tutorial_Island and Village_Human have one. If it ever must be non-destructive, the
// fix is to capture their settings as the Clean end - a change to where the left column comes from,
// not to the architecture.
//
// ---- THE BLOOM THRESHOLD SWEEP IS THE BEST THING IN HERE ------------------------------------
// BloomThreshold runs -0.5 (clean) to +1.2 (gritty). Low threshold means EVERYTHING blooms - the
// hazy, blown-out pastoral look. High threshold means only genuinely hot pixels bloom, which as the
// world darkens leaves THE FIRES THE PLAYER SET as the only things in frame that glow. The feature's
// own fiction does the lighting work. If a tuning pass ever flattens this curve, that is what is lost.
//
// ---- THE SILENT FAILURES, AND WHY DescribeOutputs() EXISTS ----------------------------------
// Two things here fail with no error at all:
//   * A DirectionalLight with Mobility Static has no dynamic representation - it is baked into
//     lightmaps, and setting Intensity or LightColor at runtime does NOTHING. Stationary is worse
//     than it looks: direct colour changes but baked indirect and shadowmaps do not, so a large
//     swing gives bright bounce light under a blood-red sun. Only Movable is fully correct. We check
//     at BeginPlay, log loudly ONCE, and never call SetMobility on a registered component.
//   * Writing an FPostProcessSettings field without setting its matching bOverride_ flag is a no-op,
//     the same class of bug as GSHordeOrderMarker.cpp:150's missing material parameter.
// Neither is visible from a log line you did not write, so DescribeOutputs() reports what the engine
// actually has rather than what we attempted.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "World/GSCorruptionDataAsset.h"
#include "GSCorruptionDirector.generated.h"

class ASkyAtmosphere;
class AExponentialHeightFog;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;

// FGSCorruptionGrade moved to GSCorruptionDataAsset.h in #337 - it is data, both this actor and
// the data asset need it, and neither owns it.


UCLASS()
class GOBLINSIEGE_API AGSCorruptionDirector : public AActor
{
	GENERATED_BODY()

public:
	AGSCorruptionDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Drive every output to this corruption level. Called at 10 Hz by the subsystem. */
	void ApplyCorruption(float Corruption01);

	/** Re-run actor discovery after streaming or a PCG spawn. */
	void RefreshDiscovery();

	/** Put every actor we touched back the way the level author had it. */
	void RestoreBaselines();

	/** What the ENGINE has, not what we attempted. See the header. */
	FString DescribeOutputs() const;

	// ---- the arc, tunable by eye ----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Grade")
	FGSCorruptionGrade CleanGrade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Grade")
	FGSCorruptionGrade GrittyGrade;

	/** Sky tint at full corruption. The clean end is whatever the level author authored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	FLinearColor CorruptRayleighScattering = FLinearColor(0.42f, 0.13f, 0.06f);

	/** Haze absorption multiplier at full corruption. Higher = thicker, dirtier air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	float CorruptMieAbsorptionScale = 6.f;

	/** Sky light contribution at full corruption. Below 1 drains the ambient blue out of shadows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	float CorruptSkyLuminanceFactor = 0.35f;

	/** Multi-scattering at full corruption. Lower = harsher, less softly-lit air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	float CorruptMultiScatteringFactor = 0.25f;

	/**
	 * OZONE, and it is the reason the first version left a blue rim at the extremes of the sky.
	 * Michael, 2026-08-26: "the sky went dark orange except for the extreme of the skybox which was
	 * blue". Driving RayleighScattering red-brown recolours the bulk of the dome but NOT the
	 * ozone layer, whose absorption defaults to a blue-cyan tint and dominates at the zenith and at
	 * grazing angles - so the blue survives however red everything else goes. Warm absorption here
	 * is what closes the dome.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	FLinearColor CorruptOtherAbsorption = FLinearColor(0.45f, 0.26f, 0.14f);

	/**
	 * Ozone absorption strength at full corruption - an ABSOLUTE target, not a multiple of whatever
	 * the level authored. It was written as a multiplier first, and L_Tutorial_Island proved that
	 * wrong: it authors OtherAbsorptionScale at 0.0, so Lerp(0, 0 * 2.0, T) is zero at every
	 * corruption level and the whole ozone fix was silently inert on that map. Multiplying a
	 * baseline that is legitimately zero can never lift it.
	 *
	 * Engine UIMax for this property is 0.2; 0.06 is a firm but not absurd haze.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky",
		meta = (ClampMin = "0.0", UIMax = "0.2"))
	float CorruptOtherAbsorptionScale = 0.06f;

	/** Haze colour at full corruption - smoke rather than clean air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sky")
	FLinearColor CorruptMieScattering = FLinearColor(0.05f, 0.032f, 0.024f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Fog")
	float CorruptFogDensityMultiplier = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Fog")
	FLinearColor CorruptFogInscatteringColor = FLinearColor(0.28f, 0.11f, 0.07f);

	/** Density given to a fog actor we had to spawn ourselves, at corruption 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Fog")
	float SpawnedFogBaseDensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sun")
	FLinearColor CorruptSunColor = FLinearColor(1.f, 0.42f, 0.22f);

	/** Sun intensity multiplier at full corruption - the sun struggles through the smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sun")
	float CorruptSunIntensityScale = 0.45f;

	/**
	 * THE SUN DISC ITSELF, which is a different parameter from the light colour and is why the
	 * first version lit the world warm while the disc stayed yellow. Michael, 2026-08-26: "the sun
	 * was the same yellow color". SetLightColor changes what the sun DOES to the scene;
	 * UDirectionalLightComponent::AtmosphereSunDiskColorScale changes what the sun LOOKS like in
	 * the sky. Both are needed, and only the second one is visible when you look up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoblinSiege|Corruption|Sun")
	FLinearColor CorruptSunDiskColorScale = FLinearColor(1.0f, 0.15f, 0.04f);

private:
	void DiscoverActors();
	void CaptureBaselines();
	void EnsurePostProcessVolume();
	void ReportSilentFailures();

	static FGSCorruptionGrade BlendGrade(const FGSCorruptionGrade& A, const FGSCorruptionGrade& B, float T);
	void ApplyGrade(const FGSCorruptionGrade& Grade);

	// Plain weak pointers, no UPROPERTY: nothing here is replicated, serialised or Blueprint-visible,
	// and TWeakObjectPtr is safe without reflection. Same rule GSRaidDirector.h states for its own
	// private state.
	TWeakObjectPtr<ASkyAtmosphere>        SkyAtmosphere;
	TWeakObjectPtr<AExponentialHeightFog> HeightFog;
	TWeakObjectPtr<ADirectionalLight>     SunLight;
	TWeakObjectPtr<ASkyLight>             SkyLightActor;
	TWeakObjectPtr<APostProcessVolume>    GradeVolume;

	/**
	 * Captured at BeginPlay. Everything lerps FROM these, never from an absolute - otherwise the
	 * first tick throws away whatever the level author authored and there is no way back.
	 */
	FLinearColor BaseRayleighScattering = FLinearColor::White;
	float BaseMieAbsorptionScale = 1.f;
	float BaseMultiScatteringFactor = 1.f;
	FLinearColor BaseSkyLuminanceFactor = FLinearColor::White;
	FLinearColor BaseOtherAbsorption = FLinearColor::White;
	float BaseOtherAbsorptionScale = 1.f;
	FLinearColor BaseMieScattering = FLinearColor::White;
	FLinearColor BaseSunDiskColorScale = FLinearColor::White;
	float BaseFogDensity = 0.02f;
	FLinearColor BaseFogInscatteringColor = FLinearColor::White;
	FLinearColor BaseSunColor = FLinearColor::White;
	float BaseSunIntensity = 3.14f;

	bool bBaselinesCaptured = false;
	bool bSpawnedFogOurselves = false;
	bool bSpawnedVolumeOurselves = false;
	bool bSunMobilityWarned = false;
	float LastAppliedCorruption01 = -1.f;
};
