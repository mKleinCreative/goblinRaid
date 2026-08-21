#include "Characters/GSCharacterBase.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Combat/GSEngagementComponent.h"
#include "Combat/GSGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Core/GSGameMode.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/GSCharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

// PHASE 2A IS MOVEMENT-NEUTRAL ON PURPOSE (Michael, 2026-08-21).
//
// AACFCharacter's constructor swaps the movement component for a UACFCharacterMovementComponent
// (ACFCharacter.cpp:64). That component is NOT a passive upgrade: TickComponent -> UpdateLocomotion
// classifies the pawn's velocity into a locomotion band every frame and rewrites MaxWalkSpeed from
// its LocomotionStates table, which ships populated - Idle 0, Walk 250, Jog 500, Sprint 650, with
// DefaultState = EJog.
//
// The consequence, measured the moment the reparent shipped: SPRINT BECOMES STRUCTURALLY
// IMPOSSIBLE. Reaching the Sprint band needs velocity above 505, MaxWalkSpeed is pinned at 500, so
// the band can never be entered. ACF expects sprint to be SetLocomotionState(ESprint) - a state
// change - not a speed write. And it is not only sprint: every move-speed effect this project has
// (the block slow, the carry slow, per-swing MoveSpeedScale, ApplyMoveSpeed itself) was being
// overwritten every frame.
//
// So we undo the swap and keep the plain UCharacterMovementComponent. This is a DEFERRAL, not a
// rejection: ruling 27 already says move speed becomes ACF locomotion states, and Phase 2b does that
// properly alongside the ARS attribute migration - deciding once how carry + block + swing compose,
// rather than twice.
//
// Safe to do: AACFCharacter::PostInitProperties only logs a warning when the cast fails
// (ACFCharacter.cpp:107), and every other use of LocomotionComp in that class is null-guarded.
// Expect one "Your Character Movement component MUST BE an ACFCharacterMovementComponent" warning
// per character until 2b. It is noise, not a failure.
AGSCharacterBase::AGSCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGSCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// ---- TICK, WHICH ACF TURNS OFF (#223, 2026-08-21) ------------------------------------------
	//
	// AACFCharacter's constructor sets `PrimaryActorTick.bStartWithTickEnabled = false`
	// (ACFCharacter.cpp:83) and NOTHING in ACF ever turns it back on. The engine default is TRUE, so
	// this is ACF changing behaviour out from under anything that derives from it.
	//
	// The symptom is not subtle and it is not obviously about tick: SPRINT AND STAMINA BOTH STOP
	// DEAD. Both live in BP_GSPlayerCharacter's Event Tick - sprint selects between BaseWalkSpeed and
	// SprintSpeed and writes MaxWalkSpeed, stamina drains and regenerates - so an actor that can tick
	// but starts with ticking disabled loses both at once, while everything event-driven (attacks,
	// dodge, interaction, death) keeps working perfectly. That combination reads as "sprint is
	// broken" rather than "the actor is not ticking", and it cost a PIE session and three wrong
	// theories about MaxWalkSpeed before the constructor was read.
	//
	// This is #135 repeating: "a subclass constructor silently disabling the tick cost two shipped
	// features". Same failure, opposite direction - the base class did it this time.
	//
	// Set on the BASE so every character gets it. AGSPlayerCharacter already sets bCanEverTick = true
	// and that was never the problem: CAN tick and STARTS ticking are different flags.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// NO ASC CREATED HERE ANY MORE (#223). AACFCharacter's constructor already builds ActionsComp,
	// and GetAbilitySystemComponent() returns it. Creating a second one is the two-health-bars bug.
	// AbilitySystemComponent is cached from it in PostInitializeComponents.

	// The attribute set STAYS a default subobject of the actor, which is exactly how it reaches the
	// ASC: UAbilitySystemComponent::InitializeComponent walks the owner's default subobjects and
	// registers every UAttributeSet it finds. That is why this worked before the reparent and why it
	// keeps working after - ACF's ActionsComp adopts it without being told.
	AttributeSetBase = CreateDefaultSubobject<UGSAttributeSetBase>(TEXT("AttributeSetBase"));

	// ACF'S OWN INITIALISER IS OFF IN PHASE 2A.
	//
	// bAutoInit defaults to TRUE, which makes UACFCharacterInitializerComponent apply a
	// UACFCharacterDataAsset at BeginPlay. We have not authored one for a single character, and the
	// acf-core skill's failure table names the result exactly: "Characters have no stats / zero
	// health - CharacterRow is empty or points to a missing DataTable row." Turning this off keeps
	// ACF's StatisticsComp dormant while OUR UGSAttributeSetBase remains the only live attribute
	// source. Phase 2b authors the DataAssets and turns it back on.
	SetAutoInit(false);

	// On the base, so the player is rationed by exactly the same rules as everyone else. A crowd
	// that visibly takes turns on a militiaman but mobs the player would be the first thing anyone
	// noticed - and the token budget is also what stops FOUR defenders deleting the player in a
	// second, which is the same bug pointed the other way.
	EngagementComponent = CreateDefaultSubobject<UGSEngagementComponent>(TEXT("EngagementComponent"));

	// ---- BODIES DO NOT PUSH THE CAMERA AROUND (#145) -------------------------------------------
	//
	// Michael, 2026-08-13, watching a ten-goblin scrum: "the camera for the player zooms in and out
	// though. We shouldn't zoom in when we're in a crowd, it makes it hard to see or comprehend
	// what's going on."
	//
	// Cause, read off the live objects rather than guessed: AGSPlayerCharacter's spring arm probes on
	// ECC_Camera with a 12uu probe over a 450uu arm, and BOTH of a character's collision primitives
	// block that channel by default - the capsule through the `Pawn` profile and the mesh through
	// `CharacterMesh`. So every body that crosses the line between the camera and the player drags the
	// boom in and lets it back out again. One passer-by is a shrug; ten goblins in a melee is a camera
	// that never stops moving, at exactly the moment the player most needs to read the fight.
	//
	// The fix belongs HERE, on the base, not on the player: the offenders are the OTHER characters.
	// The spring arm already ignores its own owner, so the player was never occluding himself - it is
	// the crowd he is standing in that does it, and that crowd is every defender and every horde
	// goblin. One line on the shared base covers all of them and cannot be forgotten on a new
	// adversary Blueprint.
	//
	// SAFE because nothing in this project traces on ECC_Camera - grepped for ECC_Camera,
	// ProbeChannel and Camera trace channels across the whole module before touching this, and the
	// only consumer is the spring arm itself. Line of sight for perception uses the sight sense's own
	// channel, and the melee sweeps use ECC_Pawn.
	//
	// World geometry still blocks the probe, so the camera continues to pull in at walls and doorways,
	// which is the behaviour bDoCollisionTest exists for.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void AGSCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: init ASC actor info now that we have a controller (needed for player-owned
	// abilities); AI-controlled defenders also route through here via their AIController.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AGSCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client: PlayerState arrives after possession on the owning client - re-init ASC actor info here.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

UAbilitySystemComponent* AGSCharacterBase::GetAbilitySystemComponent() const
{
	return Super::GetAbilitySystemComponent();
}

void AGSCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// One ASC, and it is ACF's. Cached here rather than in BeginPlay because PossessedBy can fire
	// first on a server-spawned pawn, and that path dereferences this pointer immediately.
	AbilitySystemComponent = Super::GetAbilitySystemComponent();
}

void AGSCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		BaseWalkSpeed = MoveComp->MaxWalkSpeed;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAttributeSetBase::GetHealthAttribute())
			.AddUObject(this, &AGSCharacterBase::HandleHealthChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute())
			.AddUObject(this, &AGSCharacterBase::HandleMoveSpeedMultiplierChanged);
	}

	ApplyMoveSpeed();
}

void AGSCharacterBase::HandleMoveSpeedMultiplierChanged(const FOnAttributeChangeData& /*Data*/)
{
	ApplyMoveSpeed();
}

void AGSCharacterBase::ApplyMoveSpeed()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !AbilitySystemComponent)
	{
		return;
	}

	// UGSAttributeSetBase::PreAttributeChange clamps this to [0.1, 3.0], so a stack of slows can
	// never hard-freeze a character.
	MoveComp->MaxWalkSpeed = BaseWalkSpeed * AbilitySystemComponent->GetNumericAttribute(
		UGSAttributeSetBase::GetMoveSpeedMultiplierAttribute());
}

bool AGSCharacterBase::IsHostileTo(const AActor* Other) const
{
	const AGSCharacterBase* OtherChar = Cast<AGSCharacterBase>(Other);
	if (!OtherChar)
	{
		// Not a GS character - a destructible, a barrel, a prop. Nothing to be friendly with.
		return true;
	}

	// An unset race on either side means "no opinion", and no opinion must not silently make a
	// pawn immune. Only two DECLARED and EQUAL races count as friendly.
	if (!RaceTag.IsValid() || !OtherChar->RaceTag.IsValid())
	{
		return true;
	}

	return RaceTag != OtherChar->RaceTag;
}

void AGSCharacterBase::SetBaseWalkSpeed(float NewBaseWalkSpeed)
{
	BaseWalkSpeed = NewBaseWalkSpeed;
	ApplyMoveSpeed();
}

void AGSCharacterBase::InitializeAttributesFromEffect(TSubclassOf<UGameplayEffect> InitEffectClass, float Level)
{
	if (!InitEffectClass || !AbilitySystemComponent || bAttributesInitialized)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitEffectClass, Level, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bAttributesInitialized = true;
	}
}

float AGSCharacterBase::GetHealth() const
{
	return AttributeSetBase ? AttributeSetBase->GetHealth() : 0.f;
}

float AGSCharacterBase::GetMaxHealth() const
{
	return AttributeSetBase ? AttributeSetBase->GetMaxHealth() : 0.f;
}

bool AGSCharacterBase::IsBlocking() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_Blocking);
}

bool AGSCharacterBase::IsRecoiling() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_Recoil);
}

void AGSCharacterBase::NotifyAttackWasBlocked(AActor* Blocker, float RecoilSeconds)
{
	if (!AbilitySystemComponent || bIsDead || RecoilSeconds <= 0.f)
	{
		return;
	}

	// Set rather than Add: this is a state with a deadline, not a stack. Two of your swings blocked
	// in quick succession should refresh one window, not queue two that outlive the fight.
	AbilitySystemComponent->SetLooseGameplayTagCount(GSTags::State_Recoil, 1);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// CANCEL NEXT FRAME, NOT NOW. We are currently inside the blocked swing's own
	// ApplyGameplayEffectSpecToTarget call, which is itself inside UGSGA_SwordLight::DoSweep's loop
	// over its overlap results. Cancelling the ability here re-enters EndAbility, clears the timers
	// and resets CurrentStage while that loop is still running - so the sweep would carry on
	// iterating against an ability that has already torn its own state down. The loose tag above
	// already stops anything NEW from starting, so one frame of delay costs nothing.
	TWeakObjectPtr<AGSCharacterBase> WeakSelf(this);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakSelf]()
	{
		if (AGSCharacterBase* Self = WeakSelf.Get())
		{
			if (UAbilitySystemComponent* ASC = Self->AbilitySystemComponent)
			{
				// By TAG, never CancelAbilities(nullptr) - that would also take a dodge the player
				// started on the same frame to escape this very punish.
				FGameplayTagContainer CancelTags;
				CancelTags.AddTag(GSTags::State_Attacking);
				ASC->CancelAbilities(&CancelTags);
			}
		}
	}));

	// Clear the window. Weak against this actor so a fighter killed during their own recoil does not
	// keep a timer alive, and so the tag cannot outlive them onto a respawn.
	FTimerHandle RecoilHandle;
	World->GetTimerManager().SetTimer(RecoilHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakSelf]()
		{
			if (AGSCharacterBase* Self = WeakSelf.Get())
			{
				if (UAbilitySystemComponent* ASC = Self->AbilitySystemComponent)
				{
					ASC->SetLooseGameplayTagCount(GSTags::State_Recoil, 0);
				}
			}
		}),
		RecoilSeconds, false);

	// Sell it. The flinch is what tells a spectator eight metres away that the clang mattered -
	// without it a blocked hit and a whiff look identical from outside, which the research calls the
	// most common failure in NPC-vs-NPC melee.
	if (IsValid(Blocker))
	{
		PlayHitReact(Blocker->GetActorLocation() - GetActorLocation(), Blocker);
	}
}

// GS.Combat.LogHitReact 1 makes every flinch decision say what it decided and why. A flinch that
// does not appear is otherwise indistinguishable from one that played and ended before you looked,
// which cost most of an evening on 2026-08-03.
static TAutoConsoleVariable<int32> CVarGSLogHitReact(
	TEXT("GS.Combat.LogHitReact"), 0,
	TEXT("Log every hit-reaction decision, including why one was skipped."),
	ECVF_Cheat);

#define GS_HITREACT_LOG(Format, ...) \
	if (CVarGSLogHitReact.GetValueOnGameThread() != 0) \
	{ \
		UE_LOG(LogTemp, Warning, TEXT("[GS.HitReact] %s: ") Format, *GetName(), ##__VA_ARGS__); \
	}

float AGSCharacterBase::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	// The one gate every montage in the game passes through - abilities, flinches, the horn, the
	// torch throw - because the mismatch is a property of the character, not of any one caller.
	if (AnimMontage)
	{
		const USkeletalMeshComponent* MeshComp = GetMesh();
		const USkeletalMesh* SkelMesh = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
		const USkeleton* MySkeleton = SkelMesh ? SkelMesh->GetSkeleton() : nullptr;
		const USkeleton* MontageSkeleton = AnimMontage->GetSkeleton();

		// Only refuse when BOTH are known and they differ. An unknown skeleton is not evidence of a
		// mismatch, and refusing on it would silently kill the player's animations too.
		if (MySkeleton && MontageSkeleton && MySkeleton != MontageSkeleton)
		{
			GS_HITREACT_LOG(TEXT("REFUSED montage %s: authored for %s, this character is %s"),
				*AnimMontage->GetName(), *MontageSkeleton->GetName(), *MySkeleton->GetName());
			return 0.f;
		}
	}

	return Super::PlayAnimMontage(AnimMontage, InPlayRate, StartSectionName);
}

void AGSCharacterBase::PlayHitReact(const FVector& FromDirection, AActor* Causer)
{
	// Record authorship BEFORE the tag goes on, so nothing can read the tag with a stale causer
	// still attached from the previous flinch (#221).
	if (UGSEngagementComponent* Engagement = FindComponentByClass<UGSEngagementComponent>())
	{
		Engagement->NoteFlinchCausedBy(Causer);
	}

	if (!bEnableHitReact || bIsDead)
	{
		GS_HITREACT_LOG(TEXT("disabled (bEnableHitReact %d, bIsDead %d)"),
			bEnableHitReact ? 1 : 0, bIsDead ? 1 : 0);
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Two gates, and they do different jobs. The cooldown stops a combo restarting the montage on
	// every contact - three hits in 2.1s should read as three staggers, not as a vibration. The
	// tag check stops a flinch interrupting itself mid-play, which looks worse than no flinch.
	if (World->GetTimeSeconds() - LastHitReactTime < HitReactCooldownSeconds)
	{
		GS_HITREACT_LOG(TEXT("cooldown: %.2fs since last, need %.2f"),
			World->GetTimeSeconds() - LastHitReactTime, HitReactCooldownSeconds);
		return;
	}
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(GSTags::State_HitReact))
	{
		GS_HITREACT_LOG(TEXT("already flinching (State.HitReact held)"));
		return;
	}

	UAnimMontage* Chosen = nullptr;

	if (IsBlocking() && BlockReact)
	{
		// A blocked hit is absorbed by the guard, not felt in the body.
		Chosen = BlockReact;
	}
	else
	{
		Chosen = HitReactFront;

		// Directional pick. FromDirection points from us toward the attacker, so a positive dot
		// with our right vector means the blow came from our right.
		if (!FromDirection.IsNearlyZero())
		{
			FVector Flat = FromDirection;
			Flat.Z = 0.f;
			if (!Flat.IsNearlyZero())
			{
				const float SideDot = FVector::DotProduct(Flat.GetSafeNormal(), GetActorRightVector());
				if (SideDot < -0.5f && HitReactLeft)
				{
					Chosen = HitReactLeft;
				}
				else if (SideDot > 0.5f && HitReactRight)
				{
					Chosen = HitReactRight;
				}
			}
		}
	}

	if (!Chosen)
	{
		GS_HITREACT_LOG(TEXT("no montage assigned"));
		return; // no montage authored yet - silently fine, the damage still landed
	}

	LastHitReactTime = World->GetTimeSeconds();
	const float Length = PlayAnimMontage(Chosen, HitReactPlayRate);
	GS_HITREACT_LOG(TEXT("playing %s at %.2fx -> length %.3f"),
		*Chosen->GetName(), HitReactPlayRate, Length);

	if (AbilitySystemComponent && Length > 0.f)
	{
		AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_HitReact);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (AbilitySystemComponent)
				{
					AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_HitReact);
				}
			}),
			Length, false);
	}
}

void AGSCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// Do NOT trust Data.OldValue here. Damage reaches Health through the IncomingDamage meta
	// attribute, which UGSAttributeSetBase drains with SetHealth() inside PostGameplayEffectExecute
	// - a base-value write. On that path GAS broadcasts the change with OldValue already equal to
	// NewValue, so Data-derived Delta is a constant zero. Death still worked because it tests
	// NewValue <= 0 absolutely; hit reactions did not, and neither would any damage-flash bound to
	// Delta. Measured 2026-08-03: dummy took 25 damage, died correctly at 0, never once flinched.
	// Tracking the previous value ourselves is correct regardless of how GAS fills the struct.
	const float Previous = (LastKnownHealth < 0.f) ? Data.OldValue : LastKnownHealth;
	const float Delta = Data.NewValue - Previous;
	LastKnownHealth = Data.NewValue;

	const float MaxHealth = GetMaxHealth();

	// The dynamic mirror of GAS's non-dynamic attribute delegate. Everything Blueprint-side - the
	// health bar above all - listens here, because the GAS delegate itself cannot be bound from
	// Blueprint or UMG at all.
	OnHealthChanged.Broadcast(Data.NewValue, MaxHealth, Delta);

	// Resolve the attacker BEFORE anything gates on damage size. This read used to live inside the
	// flinch branch below, which meant it only ran for hits big enough to stagger - fine for a
	// hit-react, useless for the horde's Frenzy rule, which has to answer "who is hitting my
	// summoner" for a chip-damage arrow just as much as for a greatclub. Moved up 2026-08-07 (#069).
	//
	// Prefer the attacker handed over by UGSAttributeSetBase. Data.GEModData is null on the path
	// damage actually takes (SetHealth is a base-value write), so it is only a fallback for any
	// future effect that modifies Health directly. Not named "Instigator": AActor already has a
	// member of that name and this module builds with warnings-as-errors.
	AActor* Attacker = PendingDamageInstigator.Get();
	if (!Attacker && Data.GEModData)
	{
		Attacker = const_cast<AActor*>(Data.GEModData->EffectSpec.GetContext().GetInstigator());
	}
	PendingDamageInstigator = nullptr;

	// Every damaging hit, no size gate. Fires before HandleDeath so a killing blow still tells the
	// horde who did it - a goblin whose summoner was just executed should still swarm the executioner.
	if (Delta < 0.f)
	{
		OnDamaged.Broadcast(Attacker, -Delta);
	}

	if (!bIsDead && Data.NewValue <= 0.f)
	{
		HandleDeath();
		return;
	}

	// Flinch on meaningful damage only. The fraction gate is what keeps a burning wheat field
	// from making everyone standing in it twitch four times a second.
	if (Delta < 0.f && MaxHealth > 0.f && (-Delta / MaxHealth) >= HitReactMinDamageFraction)
	{
		const FVector FromAttacker = Attacker
			? (Attacker->GetActorLocation() - GetActorLocation())
			: FVector::ZeroVector;
		GS_HITREACT_LOG(TEXT("damage %.1f of %.0f -> requesting flinch (attacker dir %s)"),
			-Delta, MaxHealth, *FromAttacker.ToCompactString());
		PlayHitReact(FromAttacker, Attacker);   // #221: the flinch names its author
	}
}

void AGSCharacterBase::NotifyDealtDamage(AActor* Victim)
{
	if (Victim && Victim != this)
	{
		OnDealtDamage.Broadcast(Victim);
	}
}

// ---- combat verbs (hoisted from AGSEnemyCharacter 2026-08-07, #069) -----------------------

void AGSCharacterBase::GrantCombatAbilities()
{
	if (!HasAuthority())
	{
		return;
	}
	GrantIfSet(LightAttackAbilityClass);
	GrantIfSet(HeavyAttackAbilityClass);
	GrantIfSet(GuardBreakAbilityClass);
	GrantIfSet(BlockAbilityClass);
	GrantIfSet(RangedAttackAbilityClass);
}

bool AGSCharacterBase::TryRangedAttack()
{
	return TryActivate(RangedAttackAbilityClass);
}

void AGSCharacterBase::GrantIfSet(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilityClass && AbilitySystemComponent)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

bool AGSCharacterBase::TryActivate(TSubclassOf<UGameplayAbility> AbilityClass)
{
	return AbilityClass && AbilitySystemComponent
		&& AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
}

bool AGSCharacterBase::TryLightAttack()
{
	// No extra gating here on purpose. GAS already refuses to re-activate an ability that is
	// running, and UGSGA_SwordLight treats that refusal as the combo buffer - so an AI that
	// spams this gets the same chained combo a player gets by mashing, for free. A BT task must
	// therefore treat a false return as "not yet", never as failure - see BTTask_MeleeAttack.
	return TryActivate(LightAttackAbilityClass);
}

bool AGSCharacterBase::TryHeavyAttack()
{
	return TryActivate(HeavyAttackAbilityClass);
}

bool AGSCharacterBase::TryGuardBreak()
{
	return TryActivate(GuardBreakAbilityClass);
}

bool AGSCharacterBase::StartBlocking()
{
	return TryActivate(BlockAbilityClass);
}

void AGSCharacterBase::StopBlocking()
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	// By tag, never CancelAbilities(nullptr) - that would also kill a swing or a dodge in flight.
	FGameplayTagContainer BlockTags;
	BlockTags.AddTag(GSTags::State_Blocking);
	AbilitySystemComponent->CancelAbilities(&BlockTags);
}

void AGSCharacterBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// 2026-08-02: this used to be a comment saying ragdoll belonged in subclasses. Nothing ever
	// implemented it, so until today death was invisible AND you kept full control of your corpse -
	// you could walk around at 0 HP indefinitely. Three things happen here now.
	//
	// 1. The State.Dead loose tag. This is the one with reach: AGSFireVolume::ApplyFireDamageTo
	//    already checks it (GSFireVolume.cpp:367) and skips corpses, so adding the tag retroactively
	//    makes an existing guard start working. Loose rather than a GameplayEffect because a corpse
	//    should never lose the tag to a duration expiring.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(GSTags::State_Dead);
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 2. Stop driving the corpse. Movement off before physics, or the movement component keeps
	//    fighting the simulation and the body skates.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
	if (AController* C = GetController())
	{
		C->SetIgnoreMoveInput(true);
		C->SetIgnoreLookInput(true);
	}

	// 3. Ragdoll. Capsule collision must go first: leave it blocking and the capsule holds the
	//    corpse up in the air while the mesh flops underneath it, which is the classic "floating
	//    dead body" bug. Mesh needs a PhysicsAsset - if one is missing the engine warns and the
	//    body simply stays in its last pose, which is survivable rather than fatal.
	if (bRagdollOnDeath)
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			MeshComp->SetAllBodiesSimulatePhysics(true);
			MeshComp->SetSimulatePhysics(true);
			MeshComp->WakeAllRigidBodies();
			MeshComp->bBlendPhysics = true;

			// A simulating skeletal mesh normally derives its bounds from where the physics bodies
			// actually are. That is correct right up until the physics asset does not fit the mesh -
			// GOB_Scout_v3 currently points at /Game/_Import/SK_GoblinScout_Rigged_v2_PhysicsAsset,
			// authored for a different body - and then the bodies start interpenetrating, depenetration
			// throws them, and the corpse's bounds measured 1881x1252x609 centred 1850uu away from the
			// pawn (2026-08-04).
			//
			// That is not just ugly. Bounds are what MoveToActor resolves "have I arrived" against, so
			// every AI within ~50m of a corpse got AlreadyAtGoal, and they are what SpawnActor's
			// collision test sees, so a body lying on the PlayerStart refused the respawn. Fixed
			// bounds keep a broken ragdoll's blast radius to itself. The physics asset is still the
			// real fix and the corpse may cull early if it slides a long way; a mis-culled corpse is a
			// far cheaper bug than an unspawnable player.
			MeshComp->bComponentUseFixedSkelBounds = true;

			// Nothing should inherit the living pawn's velocity into the ragdoll - that is a separate
			// source of first-frame launch, and it is free to rule out.
			MeshComp->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
			MeshComp->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);

			if (DeathImpulse > 0.f)
			{
				MeshComp->AddImpulse(GetActorForwardVector() * -DeathImpulse, NAME_None, true);
			}
		}
	}

	if (CorpseLifespan > 0.f)
	{
		SetLifeSpan(CorpseLifespan);
	}

	OnDied.Broadcast();

	if (HasAuthority())
	{
		if (AGSGameMode* GM = GetWorld()->GetAuthGameMode<AGSGameMode>())
		{
			GM->HandleGoblinDeath(this, GetController());
		}
	}
}

void AGSCharacterBase::KillOutright()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSetBase || bIsDead)
	{
		return;
	}

	// Zeroing the attribute rather than applying a damage effect, deliberately: this is a
	// non-combat death (a debug command, a drowning) with no instigator, no damage type and nothing
	// for UGSDamageExecCalculation's armour or blocking rules to act on. Routing it through damage
	// would invite a raised shield to survive a drowning.
	AbilitySystemComponent->SetNumericAttributeBase(UGSAttributeSetBase::GetHealthAttribute(), 0.f);
}

void AGSCharacterBase::DebugKill()
{
	if (bIsDead)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] GS.Raid.Kill: %s (hp %.0f -> 0)"),
		*GetName(), GetHealth());

	KillOutright();
}

void AGSCharacterBase::ApplyRespawnState(float HealthFraction, float InvulnerabilitySeconds)
{
	bIsDead = false;

	// Drop the health sample so the respawn refill is not measured against the corpse's zero.
	LastKnownHealth = -1.f;

	if (AttributeSetBase)
	{
		AttributeSetBase->SetHealth(AttributeSetBase->GetMaxHealth() * FMath::Clamp(HealthFraction, 0.f, 1.f));
	}

	// 2026-08-02: HandleDeath now ragdolls and locks input, so respawn has to put all of that back.
	// Before today this function only reset bIsDead and the health number, which was correct when
	// death did nothing - and would now hand you a respawned character who is a limp body with no
	// controls. Order matters: un-simulate before re-enabling the capsule, or the mesh snaps to the
	// capsule while still simulating and the character launches.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(GSTags::State_Dead);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->IsSimulatingPhysics())
		{
			MeshComp->SetSimulatePhysics(false);
			MeshComp->bBlendPhysics = false;
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
			MeshComp->AttachToComponent(GetCapsuleComponent(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			MeshComp->SetRelativeTransform(
				GetClass()->GetDefaultObject<ACharacter>()->GetMesh()->GetRelativeTransform());
		}
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	if (AController* C = GetController())
	{
		C->SetIgnoreMoveInput(false);
		C->SetIgnoreLookInput(false);
	}

	// A short-duration GameplayEffect granting a "State.Invulnerable" tag (checked by the damage
	// GameplayEffect's application requirements) is the idiomatic GAS way to implement the
	// invulnerability window - apply it here once that effect class exists as a data asset.
}

void AGSCharacterBase::SetTurnRateRadPerSec(float NewTurnRateRadPerSec)
{
	TurnRateRadPerSec = NewTurnRateRadPerSec;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->RotationRate = FRotator(0.f, FMath::RadiansToDegrees(TurnRateRadPerSec), 0.f);
	}
}
