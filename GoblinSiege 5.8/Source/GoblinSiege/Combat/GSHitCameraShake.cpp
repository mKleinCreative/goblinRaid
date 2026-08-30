#include "Combat/GSHitCameraShake.h"
// Lives in the EngineCameras plugin under Shakes/, not in core Engine - named in GoblinSiege.Build.cs.
#include "Shakes/PerlinNoiseCameraShakePattern.h"

// Numbers are FEEL, tuned to sit under hitstop: the shake should peak inside the 0.06s freeze and be
// gone before the recovery ends, so it reads as the impact rather than as something happening to the
// camera afterwards. Amplitudes are in degrees (rotation) - small on purpose; a hit shake that is
// visible as a shake has already gone too far.

UGSHitCameraShake_Light::UGSHitCameraShake_Light(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPerlinNoiseCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = false;   // stack: three quick lights should build, not gate each other
	if (UPerlinNoiseCameraShakePattern* P = Cast<UPerlinNoiseCameraShakePattern>(GetRootShakePattern()))
	{
		P->Duration = 0.12f;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = 0.08f;
		P->Pitch.Amplitude = 0.6f;  P->Pitch.Frequency = 40.f;
		P->Yaw.Amplitude   = 0.3f;  P->Yaw.Frequency   = 35.f;
		P->Roll.Amplitude  = 0.f;
	}
}

UGSHitCameraShake_Heavy::UGSHitCameraShake_Heavy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPerlinNoiseCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = true;    // a heavy restarts its own shake rather than piling up
	if (UPerlinNoiseCameraShakePattern* P = Cast<UPerlinNoiseCameraShakePattern>(GetRootShakePattern()))
	{
		P->Duration = 0.22f;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = 0.14f;
		P->Pitch.Amplitude = 1.4f;  P->Pitch.Frequency = 30.f;
		P->Yaw.Amplitude   = 0.7f;  P->Yaw.Frequency   = 25.f;
		P->Roll.Amplitude  = 0.8f;  P->Roll.Frequency  = 20.f;
	}
}
