#include "World/GSCorruptionDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Components/SceneComponent.h"
#include "World/GSCorruptionSubsystem.h"


AGSCorruptionDirector::AGSCorruptionDirector()
{
	// Driven by the subsystem's single timer. A per-frame tick here would be a second clock for the
	// same value - and AACFCharacter's bStartWithTickEnabled = false trap is a standing reminder in
	// this project that a tick you did not verify is a tick that may not run.
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// ---- the clean end: bloom-heavy and almost blown out --------------------------------------
	CleanGrade.BloomIntensity   = 3.0f;
	CleanGrade.BloomThreshold   = -0.5f;
	CleanGrade.AutoExposureBias = 0.75f;
	CleanGrade.Saturation       = 1.05f;
	CleanGrade.Contrast         = 0.90f;
	CleanGrade.Gamma            = 1.08f;
	CleanGrade.Gain             = FLinearColor(1.f, 1.f, 1.f);
	CleanGrade.FilmGrain        = 0.f;
	CleanGrade.Vignette         = 0.15f;
	CleanGrade.SceneFringe      = 0.f;

	// ---- the gritty end: dark, harsh, dirty ---------------------------------------------------
	GrittyGrade.BloomIntensity   = 0.35f;
	GrittyGrade.BloomThreshold   = 1.20f;
	GrittyGrade.AutoExposureBias = -0.40f;
	GrittyGrade.Saturation       = 0.55f;
	GrittyGrade.Contrast         = 1.25f;
	GrittyGrade.Gamma            = 0.92f;
	GrittyGrade.Gain             = FLinearColor(1.05f, 0.92f, 0.85f);
	GrittyGrade.FilmGrain        = 0.60f;
	GrittyGrade.Vignette         = 0.60f;
	GrittyGrade.SceneFringe      = 0.80f;
}

void AGSCorruptionDirector::BeginPlay()
{
	Super::BeginPlay();

	// The output half is cosmetic. A dedicated server has nothing to render and no local player to
	// render it for - but note the SUBSYSTEM still exists there, because its driver half is
	// authoritative gameplay state. The split lives here, not in ShouldCreateSubsystem.
	if (IsRunningDedicatedServer())
	{
		UE_LOG(LogGSCorruption, Log, TEXT("Dedicated server - corruption output stage is inert."));
		return;
	}

	DiscoverActors();
	CaptureBaselines();
	EnsurePostProcessVolume();
	ReportSilentFailures();

	ApplyCorruption(0.f);
}

void AGSCorruptionDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreBaselines();
	Super::EndPlay(EndPlayReason);
}

void AGSCorruptionDirector::RefreshDiscovery()
{
	DiscoverActors();
	CaptureBaselines();
	EnsurePostProcessVolume();
	ReportSilentFailures();
}

void AGSCorruptionDirector::DiscoverActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!SkyAtmosphere.IsValid())
	{
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) { SkyAtmosphere = *It; break; }
	}
	if (!HeightFog.IsValid())
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { HeightFog = *It; break; }
	}
	if (!SunLight.IsValid())
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It) { SunLight = *It; break; }
	}
	if (!SkyLightActor.IsValid())
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It) { SkyLightActor = *It; break; }
	}

	// L_CombatArena has no fog actor at all, and it is the map this has to be watched in. Spawning
	// one is the difference between the feature working on the default startup map and appearing to
	// do nothing there.
	const UGSCorruptionSubsystem* Settings = UGSCorruptionSubsystem::Get(this);
	const bool bMaySpawnFog = !Settings || Settings->bSpawnMissingAtmosphereActors;

	if (!HeightFog.IsValid() && bMaySpawnFog)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		AExponentialHeightFog* Spawned =
			World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform::Identity, Params);
		if (Spawned)
		{
			HeightFog = Spawned;
			bSpawnedFogOurselves = true;
			if (UExponentialHeightFogComponent* C = Spawned->GetComponent())
			{
				C->SetFogDensity(SpawnedFogBaseDensity);
			}
			UE_LOG(LogGSCorruption, Log,
				TEXT("Map had no ExponentialHeightFog - spawned one (density %.3f). If the fog on ")
				TEXT("this map looks wrong, this line is why."), SpawnedFogBaseDensity);
		}
	}
}

void AGSCorruptionDirector::CaptureBaselines()
{
	if (bBaselinesCaptured)
	{
		return;
	}

	if (const ASkyAtmosphere* Sky = SkyAtmosphere.Get())
	{
		if (const USkyAtmosphereComponent* C = Sky->GetComponent())
		{
			BaseRayleighScattering  = C->RayleighScattering;
			BaseMieAbsorptionScale  = C->MieAbsorptionScale;
			BaseMultiScatteringFactor = C->MultiScatteringFactor;
			BaseSkyLuminanceFactor  = C->SkyLuminanceFactor;
		}
	}

	if (const AExponentialHeightFog* Fog = HeightFog.Get())
	{
		if (const UExponentialHeightFogComponent* C = Fog->GetComponent())
		{
			BaseFogDensity = C->FogDensity;
			BaseFogInscatteringColor = C->FogInscatteringLuminance;
		}
	}

	if (const ADirectionalLight* Sun = SunLight.Get())
	{
		if (const ULightComponent* C = Cast<ULightComponent>(Sun->GetLightComponent()))
		{
			BaseSunColor = C->GetLightColor();
			BaseSunIntensity = C->Intensity;
		}
	}

	bBaselinesCaptured = true;
}

void AGSCorruptionDirector::EnsurePostProcessVolume()
{
	UWorld* World = GetWorld();
	if (!World || GradeVolume.IsValid())
	{
		return;
	}

	// Spawn our OWN volume rather than editing the level artist's. Ours is unbound at priority 1000,
	// so it composes on top of whatever the map has and can be removed without touching their asset.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	APostProcessVolume* Volume =
		World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);

	if (!Volume)
	{
		UE_LOG(LogGSCorruption, Error, TEXT("Failed to spawn the corruption post-process volume."));
		return;
	}

	Volume->bUnbound = true;
	Volume->Priority = 1000.f;
	// Stays 1. The clean look is an EFFECT, not the absence of one - see the header.
	Volume->BlendWeight = 1.f;

	GradeVolume = Volume;
	bSpawnedVolumeOurselves = true;
}

void AGSCorruptionDirector::ReportSilentFailures()
{
	if (bSunMobilityWarned)
	{
		return;
	}

	if (const ADirectionalLight* Sun = SunLight.Get())
	{
		if (const USceneComponent* C = Sun->GetLightComponent())
		{
			const EComponentMobility::Type Mob = C->Mobility;
			if (Mob == EComponentMobility::Static)
			{
				UE_LOG(LogGSCorruption, Warning,
					TEXT("DirectionalLight '%s' Mobility=Static. Sun corruption WILL NOT RENDER - a ")
					TEXT("static light is baked into lightmaps and has no dynamic representation. ")
					TEXT("Set it to Movable in the level. (GS.Corruption.Dump reports this.)"),
					*Sun->GetName());
			}
			else if (Mob == EComponentMobility::Stationary)
			{
				UE_LOG(LogGSCorruption, Warning,
					TEXT("DirectionalLight '%s' Mobility=Stationary. Colour and intensity WILL change ")
					TEXT("but baked indirect and shadowmaps will not, so a large swing gives bright ")
					TEXT("bounce light under a red sun. Movable is the only fully correct setting."),
					*Sun->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogGSCorruption, Warning,
			TEXT("No DirectionalLight found. Sky, fog and the grade will still drive; the sun will not."));
	}

	bSunMobilityWarned = true;
}

FGSCorruptionGrade AGSCorruptionDirector::BlendGrade(const FGSCorruptionGrade& A, const FGSCorruptionGrade& B, float T)
{
	FGSCorruptionGrade R;
	R.BloomIntensity   = FMath::Lerp(A.BloomIntensity,   B.BloomIntensity,   T);
	R.BloomThreshold   = FMath::Lerp(A.BloomThreshold,   B.BloomThreshold,   T);
	R.AutoExposureBias = FMath::Lerp(A.AutoExposureBias, B.AutoExposureBias, T);
	R.Saturation       = FMath::Lerp(A.Saturation,       B.Saturation,       T);
	R.Contrast         = FMath::Lerp(A.Contrast,         B.Contrast,         T);
	R.Gamma            = FMath::Lerp(A.Gamma,            B.Gamma,            T);
	R.Gain             = FMath::Lerp(A.Gain,             B.Gain,             T);
	R.FilmGrain        = FMath::Lerp(A.FilmGrain,        B.FilmGrain,        T);
	R.Vignette         = FMath::Lerp(A.Vignette,         B.Vignette,         T);
	R.SceneFringe      = FMath::Lerp(A.SceneFringe,      B.SceneFringe,      T);
	return R;
}

void AGSCorruptionDirector::ApplyGrade(const FGSCorruptionGrade& Grade)
{
	APostProcessVolume* Volume = GradeVolume.Get();
	if (!Volume)
	{
		return;
	}

	FPostProcessSettings& S = Volume->Settings;

	// EVERY bOverride_ flag below is load-bearing. Writing the field without it is a silent no-op -
	// the FPostProcessSettings equivalent of setting a material parameter the material does not have.
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = Grade.BloomIntensity;

	S.bOverride_BloomThreshold = true;
	S.BloomThreshold = Grade.BloomThreshold;

	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = Grade.AutoExposureBias;

	// The four grading fields are FVector4, not float: XYZ is per-channel RGB and W is the master
	// multiplier. Treating them as scalars compiles cleanly and writes garbage, so RGB stays at 1
	// and only W carries the curve - except Gain, which is where the ash tint belongs.
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(1.f, 1.f, 1.f, Grade.Saturation);

	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.f, 1.f, 1.f, Grade.Contrast);

	S.bOverride_ColorGamma = true;
	S.ColorGamma = FVector4(1.f, 1.f, 1.f, Grade.Gamma);

	S.bOverride_ColorGain = true;
	S.ColorGain = FVector4(Grade.Gain.R, Grade.Gain.G, Grade.Gain.B, 1.f);

	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = Grade.FilmGrain;

	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = Grade.Vignette;

	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = Grade.SceneFringe;
}

void AGSCorruptionDirector::ApplyCorruption(float Corruption01)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	const float T = FMath::Clamp(Corruption01, 0.f, 1.f);
	LastAppliedCorruption01 = T;

	ApplyGrade(BlendGrade(CleanGrade, GrittyGrade, T));

	if (ASkyAtmosphere* Sky = SkyAtmosphere.Get())
	{
		if (USkyAtmosphereComponent* C = Sky->GetComponent())
		{
			C->SetRayleighScattering(FMath::Lerp(BaseRayleighScattering, CorruptRayleighScattering, T));
			C->SetMieAbsorptionScale(FMath::Lerp(BaseMieAbsorptionScale, BaseMieAbsorptionScale * CorruptMieAbsorptionScale, T));
			C->SetMultiScatteringFactor(FMath::Lerp(BaseMultiScatteringFactor, CorruptMultiScatteringFactor, T));
			C->SetSkyLuminanceFactor(FMath::Lerp(BaseSkyLuminanceFactor, BaseSkyLuminanceFactor * CorruptSkyLuminanceFactor, T));
		}
	}

	if (AExponentialHeightFog* Fog = HeightFog.Get())
	{
		if (UExponentialHeightFogComponent* C = Fog->GetComponent())
		{
			C->SetFogDensity(FMath::Lerp(BaseFogDensity, BaseFogDensity * CorruptFogDensityMultiplier, T));
			C->SetFogInscatteringColor(FMath::Lerp(BaseFogInscatteringColor, CorruptFogInscatteringColor, T));
		}
	}

	const UGSCorruptionSubsystem* Settings = UGSCorruptionSubsystem::Get(this);
	const bool bDriveSun = !Settings || Settings->bDriveDirectionalLight;

	if (ADirectionalLight* Sun = bDriveSun ? SunLight.Get() : nullptr)
	{
		// Gated so a baked-lighting map can opt out cleanly rather than fighting it.
		if (ULightComponent* C = Sun->GetLightComponent())
		{
			C->SetLightColor(FMath::Lerp(BaseSunColor, CorruptSunColor, T));
			C->SetIntensity(FMath::Lerp(BaseSunIntensity, BaseSunIntensity * CorruptSunIntensityScale, T));
		}
	}
}

void AGSCorruptionDirector::RestoreBaselines()
{
	if (!bBaselinesCaptured)
	{
		return;
	}

	if (ASkyAtmosphere* Sky = SkyAtmosphere.Get())
	{
		if (USkyAtmosphereComponent* C = Sky->GetComponent())
		{
			C->SetRayleighScattering(BaseRayleighScattering);
			C->SetMieAbsorptionScale(BaseMieAbsorptionScale);
			C->SetMultiScatteringFactor(BaseMultiScatteringFactor);
			C->SetSkyLuminanceFactor(BaseSkyLuminanceFactor);
		}
	}

	if (AExponentialHeightFog* Fog = HeightFog.Get())
	{
		if (UExponentialHeightFogComponent* C = Fog->GetComponent())
		{
			C->SetFogDensity(BaseFogDensity);
			C->SetFogInscatteringColor(BaseFogInscatteringColor);
		}
	}

	if (ADirectionalLight* Sun = SunLight.Get())
	{
		if (ULightComponent* C = Sun->GetLightComponent())
		{
			C->SetLightColor(BaseSunColor);
			C->SetIntensity(BaseSunIntensity);
		}
	}
}

FString AGSCorruptionDirector::DescribeOutputs() const
{
	FString Out;

	if (const ASkyAtmosphere* Sky = SkyAtmosphere.Get())
	{
		Out += FString::Printf(TEXT("[GS.Corruption]   SkyAtmosphere       '%s' found\n"), *Sky->GetName());
	}
	else
	{
		Out += TEXT("[GS.Corruption]   SkyAtmosphere       NONE - the sky will not change\n");
	}

	if (const AExponentialHeightFog* Fog = HeightFog.Get())
	{
		Out += FString::Printf(TEXT("[GS.Corruption]   ExpHeightFog        '%s'%s\n"),
			*Fog->GetName(), bSpawnedFogOurselves ? TEXT("  SPAWNED BY US (map had none)") : TEXT(""));
	}
	else
	{
		Out += TEXT("[GS.Corruption]   ExpHeightFog        NONE\n");
	}

	if (const APostProcessVolume* V = GradeVolume.Get())
	{
		Out += FString::Printf(
			TEXT("[GS.Corruption]   PostProcess         ours, unbound=%d prio=%.0f blend=%.2f  bloom=%.2f thresh=%.2f grain=%.2f sat=%.2f\n"),
			V->bUnbound ? 1 : 0, V->Priority, V->BlendWeight,
			V->Settings.BloomIntensity, V->Settings.BloomThreshold,
			V->Settings.FilmGrainIntensity, V->Settings.ColorSaturation.W);
	}
	else
	{
		Out += TEXT("[GS.Corruption]   PostProcess         NONE - the grade is not being applied\n");
	}

	if (const ADirectionalLight* Sun = SunLight.Get())
	{
		const USceneComponent* C = Sun->GetLightComponent();
		const EComponentMobility::Type Mob = C ? C->Mobility.GetValue() : EComponentMobility::Static;
		const TCHAR* MobName =
			(Mob == EComponentMobility::Movable) ? TEXT("Movable") :
			(Mob == EComponentMobility::Stationary) ? TEXT("Stationary") : TEXT("Static");
		const TCHAR* Verdict =
			(Mob == EComponentMobility::Movable) ? TEXT("") :
			(Mob == EComponentMobility::Stationary)
				? TEXT("  ** direct only - baked indirect will not follow **")
				: TEXT("  ** WILL NOT RENDER - set it Movable **");
		Out += FString::Printf(TEXT("[GS.Corruption]   DirectionalLight    '%s' Mobility=%s%s\n"),
			*Sun->GetName(), MobName, Verdict);
	}
	else
	{
		Out += TEXT("[GS.Corruption]   DirectionalLight    NONE\n");
	}

	if (const ASkyLight* SL = SkyLightActor.Get())
	{
		Out += FString::Printf(TEXT("[GS.Corruption]   SkyLight            '%s' found\n"), *SL->GetName());
	}
	else
	{
		Out += TEXT("[GS.Corruption]   SkyLight            NONE\n");
	}

	Out += FString::Printf(TEXT("[GS.Corruption]   last applied        %.2f\n"), LastAppliedCorruption01);

	return Out;
}
