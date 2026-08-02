#include "Destruction/GSFireVolume.h"
#include "Destruction/GSFlammableComponent.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSGE_FireDamage.h"
#include "Characters/GSEnemyCharacter.h"
#include "Core/GSGameState.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PointLightComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AGSFireVolume::AGSFireVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
	DamageSphere->InitSphereRadius(DamageRadius);
	DamageSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = DamageSphere;

	FireFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireFX"));
	FireFX->SetupAttachment(RootComponent);

	SmokeFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SmokeFX"));
	SmokeFX->SetupAttachment(RootComponent);

	EmberFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EmberFX"));
	EmberFX->SetupAttachment(RootComponent);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);
	FireLight->SetCastShadows(false);          // dusk lighting stays static - no shadow maps from fire
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 90.f)); // sit in the flame body, not on the floor
	FireLight->bUseInverseSquaredFalloff = false;            // gentler, more controllable reach

	FireDamageEffectClass = UGSGE_FireDamage::StaticClass();

	// Soft path, deliberately: if the asset is ever renamed or missing, fire still burns and still
	// damages - it just isn't drawn. A hard reference would make that a load failure instead.
	// Marketplace systems as of 2026-07-30. These replace the hand-tuned NS_GS_SurfaceFire /
	// NS_GS_SmokeColumn, which were a torch flame retuned into a ground fire - always a
	// compromise, and one I couldn't judge because there's no screenshot route from the agent.
	//
	// N_MeteorSpawn is the PEAK look, not the resting state: SetFireIntensity ramps toward it.
	FireSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/RPG-MagicVFXFire/VFX/Niagara/N_MeteorSpawn.N_MeteorSpawn")));
	SmokeSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VolcanoEnvironmentVFX/VFX/Niagara/NS_FlameSmoke.NS_FlameSmoke")));
	EmberSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VolcanoEnvironmentVFX/VFX/Niagara/NS_FlameEmbers.NS_FlameEmbers")));
}

void AGSFireVolume::SetFireIntensity(float NewIntensity01)
{
	FireIntensity01 = FMath::Clamp(NewIntensity01, 0.f, 1.f);

	// Scale is the arc: a new cell is a small flame, established ground reaches full size.
	const float Scale = FMath::Lerp(MinIntensityScale, 1.f, FireIntensity01);

	if (FireFX)
	{
		FireFX->SetRelativeScale3D(FVector(Scale));
	}
	if (SmokeFX)
	{
		// Smoke lags behind the flame - a fire that is dying still smokes heavily, so the floor is
		// high and the whole arc rides on top of SmokeBaseScale rather than replacing it. Before
		// 2026-07-31 this line overwrote the scale outright, which silently undid the plume sizing
		// on every intensity update and is why the field fire read as flames with no smoke.
		SmokeFX->SetRelativeScale3D(FVector(SmokeBaseScale * FMath::Lerp(0.8f, 1.f, FireIntensity01)));
	}
	if (EmberFX)
	{
		EmberFX->SetRelativeScale3D(FVector(Scale));
	}

	// Light tracks intensity too, so a field at full burn genuinely floods the area and a dying
	// one only glows. FlickerTick multiplies on top of this.
	BaseLightIntensity = LightIntensity * FMath::Lerp(0.35f, 1.f, FireIntensity01);
	if (FireLight && bEnableLight)
	{
		FireLight->SetIntensity(BaseLightIntensity);
	}
}

void AGSFireVolume::ApplyFireLight()
{
	if (!FireLight)
	{
		return;
	}

	if (!bEnableLight)
	{
		FireLight->SetVisibility(false);
		return;
	}

	BaseLightIntensity = LightIntensity;

	FireLight->SetLightColor(LightColour);
	FireLight->SetIntensity(LightIntensity);
	FireLight->SetAttenuationRadius(DamageRadius * LightRadiusScale);
	FireLight->SetCastShadows(false);
	FireLight->SetVisibility(true);

	if (LightFlickerAmount > 0.f)
	{
		GetWorldTimerManager().SetTimer(FlickerTimerHandle, this, &AGSFireVolume::FlickerTick, 0.05f, true);
	}
}

void AGSFireVolume::FlickerTick()
{
	if (!FireLight || !bEnableLight)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Two out-of-phase sines rather than random noise: random flicker reads as a broken bulb,
	// whereas a wobbling beat reads as fire. The per-actor offset stops several volumes on the
	// same burn front pulsing in unison, which would look mechanical.
	const float T = World->GetTimeSeconds() + GetUniqueID() * 0.37f;
	const float Wobble =
		FMath::Sin(T * LightFlickerSpeed) * 0.6f +
		FMath::Sin(T * LightFlickerSpeed * 2.37f) * 0.4f;

	FireLight->SetIntensity(BaseLightIntensity * (1.f + Wobble * LightFlickerAmount));
}

void AGSFireVolume::ApplyFireFX()
{
	if (!FireFX)
	{
		return;
	}

	UNiagaraSystem* System = FireSystem.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GoblinSiege] %s has no fire Niagara system (%s) - it will burn invisibly."),
			*GetName(), *FireSystem.ToString());
		return;
	}

	FireFX->SetAsset(System);
	FireFX->Activate(true);

	// ---- smoke ----------------------------------------------------------
	if (SmokeFX)
	{
		if (!bEnableSmoke)
		{
			SmokeFX->SetVisibility(false);
		}
		else if (UNiagaraSystem* Smoke = SmokeSystem.LoadSynchronous())
		{
			SmokeFX->SetAsset(Smoke);
			RefreshSmokeScale();
			SmokeFX->Activate(true);
		}
	}

	// ---- embers ---------------------------------------------------------
	if (EmberFX)
	{
		if (!bEnableEmbers)
		{
			EmberFX->SetVisibility(false);
		}
		else if (UNiagaraSystem* Embers = EmberSystem.LoadSynchronous())
		{
			EmberFX->SetAsset(Embers);
			EmberFX->Activate(true);
		}
	}

	// Optional legacy rescale, off by default - see bAutoScaleFXToRadius.
	if (bAutoScaleFXToRadius && FireSystemAuthoredRadius > 0.f)
	{
		const float RadiusScale = FMath::Max(0.1f, DamageRadius / FireSystemAuthoredRadius);
		FireFX->SetRelativeScale3D(FVector(RadiusScale));
		// Smoke deliberately NOT rescaled here - it owns its own sizing via SmokeBaseScale above,
		// so turning the legacy flame rescale back on can never re-bury the plume.
		if (EmberFX) { EmberFX->SetRelativeScale3D(FVector(RadiusScale)); }
	}
	else
	{
		// Apply the current intensity arc at native authored size.
		SetFireIntensity(FireIntensity01);
	}
}

void AGSFireVolume::ConfigurePooled(float InDamageRadius, bool bInEnableSmoke, bool bInEnableLight)
{
	// Must be called before BeginPlay (between SpawnActorDeferred and FinishSpawning).
	bAutoExpire = false;
	bSpreadToFlammables = false;
	AlarmOnSpawn = 0.f;

	// 2026-07-31: the owner decides whether it wants a plume here. The field says no, because it
	// runs its own smoke over the BURNT region - smoke that stays behind after the front has moved
	// on, which a volume-attached plume can never do. Set before BeginPlay, so ApplyFireFX() simply
	// never activates SmokeFX rather than activating it and hiding it.
	bEnableSmoke = bInEnableSmoke;

	// 2026-07-31 (Q-33): and likewise for the point light, which until today every volume built
	// and lit unconditionally. Set before BeginPlay so ApplyFireLight() takes its disabled path -
	// visibility off and, critically, no flicker timer started at all - rather than lighting the
	// scene for one frame and then being switched off. See the header for why 10 flame patches
	// wanting only 3 lights is the correct reading of burn-types spec §3.4.
	bEnableLight = bInEnableLight;

	SetDamageRadius(InDamageRadius);
}

void AGSFireVolume::SetLightEnabled(bool bNewEnabled)
{
	// 2026-07-31 (Q-33). Live counterpart to the ConfigurePooled flag, for the field's pooled
	// volumes, which are repositioned rather than respawned and so outlive any decision made at
	// spawn time. Idempotent and cheap: the field calls this on every slot on every update, and a
	// no-change call must cost nothing, or the "call it each update" contract would be a per-tick
	// SetTimer/ClearTimer churn on ten actors.
	if (bEnableLight == bNewEnabled)
	{
		return;
	}

	bEnableLight = bNewEnabled;

	if (!FireLight)
	{
		return;
	}

	if (!bEnableLight)
	{
		// Clear the flicker timer as well as hiding the light. FlickerTick already early-returns on
		// !bEnableLight, so leaving it running would be harmless - and permanently wasteful, 20 Hz
		// per unlit volume for the length of the raid. This is exactly what re-running
		// ApplyFireLight() would NOT do, and why this function exists at all.
		GetWorldTimerManager().ClearTimer(FlickerTimerHandle);
		FireLight->SetVisibility(false);
		return;
	}

	// Turning back on: ApplyFireLight() re-derives colour, attenuation and BaseLightIntensity from
	// the CURRENT DamageRadius, which matters because a pooled volume may have been resized since
	// it last shone. Then re-apply the intensity arc, since BaseLightIntensity is otherwise reset
	// to the flat LightIntensity and the volume would flash to full brightness on a cell that has
	// barely caught.
	ApplyFireLight();
	SetFireIntensity(FireIntensity01);
}

void AGSFireVolume::SetDamageRadius(float NewRadius)
{
	DamageRadius = FMath::Max(0.f, NewRadius);
	if (DamageSphere)
	{
		DamageSphere->SetSphereRadius(DamageRadius, true);
	}

	// The plume is sized off the burning area, so a resize has to re-derive it.
	RefreshSmokeScale();
}

void AGSFireVolume::RefreshSmokeScale()
{
	// Size the plume off the area that actually burns, NOT off the flame system's authored size -
	// the two are unrelated, and coupling them is what buried the smoke (2026-07-31). At the field's
	// 320uu pooled radius with the defaults here, the plume comes out ~3.4x the smoke system's
	// native size instead of the ~0.6-1.0x it was rendering at, which is the whole visible fix.
	SmokeBaseScale = (bScaleSmokeToRadius && SmokeAuthoredRadius > 0.f)
		? FMath::Max(0.1f, (DamageRadius / SmokeAuthoredRadius) * SmokePlumeScale)
		: FMath::Max(0.1f, SmokePlumeScale);

	if (SmokeFX)
	{
		SmokeFX->SetRelativeScale3D(FVector(SmokeBaseScale * FMath::Lerp(0.8f, 1.f, FireIntensity01)));
	}
}

void AGSFireVolume::BeginPlay()
{
	Super::BeginPlay();

	// The constructor's InitSphereRadius used the class-default value; re-apply in case a spawner
	// (or ConfigurePooled) changed DamageRadius afterwards.
	SetDamageRadius(DamageRadius);

	// FX and light run on clients too - the only parts of the volume that aren't
	// server-authoritative.
	ApplyFireFX();
	ApplyFireLight();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DamageTickHandle, this, &AGSFireVolume::DamageTick, DamageTickInterval, true);

		if (bSpreadToFlammables)
		{
			GetWorldTimerManager().SetTimer(SpreadTimerHandle, this, &AGSFireVolume::SpreadTick, SpreadDelaySeconds, false);
		}

		if (AlarmOnSpawn > 0.f)
		{
			if (AGSGameState* GS = Cast<AGSGameState>(UGameplayStatics::GetGameState(this)))
			{
				GS->AddAlarm(AlarmOnSpawn, EGSAlarmSource::FireDamage);
			}
		}

		if (bAutoExpire)
		{
			SetLifeSpan(LifetimeSeconds);
		}
	}
}

void AGSFireVolume::DamageTick()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageSphere->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		// Friendly fire is intentionally ON (design doc §4) - no ally/goblin exclusion here, only
		// the FriendlyFireScalar applied inside ApplyFireDamageTo.
		ApplyFireDamageTo(Actor);
	}
}

void AGSFireVolume::ApplyFireDamageTo(AActor* Target)
{
	if (!Target || !FireDamageEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return; // no ASC - props and non-GAS pawns simply aren't damageable
	}

	// Dodge-roll i-frames and corpses are not valid targets.
	if (TargetASC->HasMatchingGameplayTag(GSTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(GSTags::State_Invulnerable))
	{
		return;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(this, this);

	// Applied to self on the target's ASC: this hazard has no ASC of its own, and the exec calc
	// reads armor straight off the target, so no source captures are needed.
	const FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(FireDamageEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	// Defenders take full damage; goblins and allies take the friendly-fire scalar. "Horde goblins
	// take friendly fire like everyone else... both a tragedy and the funniest thing that will
	// happen all raid" (GDD §2.5) - so this scales, it never exempts.
	const bool bIsDefender = Target->IsA(AGSEnemyCharacter::StaticClass());
	const float Scalar = bIsDefender ? 1.f : FriendlyFireScalar;
	const float RawDamage = DamagePerTick * Scalar;

	// The Damage.* tag is BOTH the type marker and the SetByCaller key - see GSDamageExecCalculation.
	Spec->AddDynamicAssetTag(GSTags::Damage_Fire);
	Spec->SetSetByCallerMagnitude(GSTags::Damage_Fire, RawDamage);

	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);

	TotalDamageDealt += RawDamage;
}

void AGSFireVolume::SpreadTick()
{
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SpreadRadius);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic), Sphere);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* OverlappedActor = Overlap.GetActor())
		{
			if (UGSFlammableComponent* Flammable = OverlappedActor->FindComponentByClass<UGSFlammableComponent>())
			{
				Flammable->Ignite();
			}
		}
	}
}
