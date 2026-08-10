// One switch for "show me what a player would see".
//
// No header on purpose - this file registers console commands and exports nothing. There is a
// sibling of this pattern at Destruction/GSBurnDebugCommands.cpp. Worth knowing: a headerless
// .cpp is invisible to any inventory that lists *.h, which is how 543 lines of existing debug
// tooling went unnoticed for a fortnight.
//
//   GS.PlayerView 1   -> hide every debug overlay this project draws
//   GS.PlayerView 0   -> put them all back
//
// It is a wrapper, not a new system: it just sets the individual cvars, so any of them can still
// be driven on its own when you are debugging one thing in isolation.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** Every debug toggle in the project, with the value that means "quiet". */
	struct FGSDebugToggle
	{
		const TCHAR* Name;
		int32 QuietValue;
		int32 LoudValue;
	};

	static const FGSDebugToggle GSDebugToggles[] =
	{
		{ TEXT("GS.Combat.Debug"),    0, 1 },  // melee trace spheres, hit markers, combo stage text
		{ TEXT("GS.Combat.LogDamage"), 0, 2 }, // per-hit damage breakdown (2 = also on screen)
		{ TEXT("GS.Burn.Debug"),      0, 1 },  // fire grid overlay and per-character HP readout
		// Added 2026-08-04 with the aim framework. Note what this does NOT hide: the arc RIBBON is a
		// shipping feature drawn with real spline meshes, and a player view is exactly where it
		// belongs. This only silences the raw predicted-path debug lines drawn alongside it.
		{ TEXT("GS.Aim.Debug"),       0, 1 },  // raw PredictProjectilePath lines behind the arc ribbon
		// These two existed before today and were simply never listed here, so GS.PlayerView 1 left
		// them talking. Found while adding the line above.
		{ TEXT("GS.Combat.LogHitReact"), 0, 1 }, // per-flinch selection log
		{ TEXT("GS.Interact.Debug"),  0, 1 },  // focus traces and channel progress readout
		// Added 2026-08-08 with NPC-vs-NPC melee. Chatty by nature - it logs a line per decision per
		// agent - so it belongs in the table more than most of the entries above it.
		{ TEXT("GS.Combat.LogAI"),    0, 1 },  // AI target/block/guard-break decision log
	};

	void GSSetPlayerView(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		// Default to the useful direction: bare "GS.PlayerView" means "make it clean".
		const bool bPlayerView = (Args.Num() == 0) || (FCString::Atoi(*Args[0]) != 0);

		int32 Applied = 0;
		for (const FGSDebugToggle& Toggle : GSDebugToggles)
		{
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Toggle.Name))
			{
				CVar->Set(bPlayerView ? Toggle.QuietValue : Toggle.LoudValue, ECVF_SetByConsole);
				++Applied;
			}
			// A missing cvar is not an error: these live in different modules and a given one may
			// simply not have been touched this session, so its static has not registered yet.
		}

		// Clear anything already on screen, or the last frame's messages linger for their duration
		// and it looks like the switch did not work.
		if (bPlayerView && GEngine)
		{
			GEngine->ClearOnScreenDebugMessages();
		}

		Ar.Logf(TEXT("GS.PlayerView %d - %d/%d debug channels set (%s)"),
			bPlayerView ? 1 : 0, Applied, UE_ARRAY_COUNT(GSDebugToggles),
			bPlayerView ? TEXT("clean") : TEXT("verbose"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSPlayerViewCmd(
	TEXT("GS.PlayerView"),
	TEXT("GS.PlayerView [0|1] - 1 (default) hides every GoblinSiege debug overlay so you can judge "
		 "the game as a player would; 0 turns them all back on."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&GSSetPlayerView));

// ------------------------------------------------------------------------ GS.Horde.* (#069)
//
// The exit test for the horde is "ten goblins plus the player brawling guards without a frame
// drop" (GDD §3.1, §4.2, repo export §12.2 block D). That is not a thing you can reach by playing
// - it needs three horn blasts, a pool that has not been spent, and arrival markers placed - so it
// gets a command, the same way the two lose paths got GS.Raid.Kill / GS.Raid.ExpireClock in #018.
//
// Deliberately routed through UGSHordeSubsystem::SummonWave rather than spawning directly: a test
// that bypassed the pool would prove the pawn renders and prove nothing about the system under
// test, and would happily produce eleven goblins against a cap of ten.

#include "Horde/GSHordeSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** The world the GAME is in, not the world the console was typed in. Same fix as
	 *  GSRaidDebug::GameWorld - the editor's Output Log console is not the PIE world, and four
	 *  attempts at GS.Raid.GotoActor produced zero output before that was understood (#027). */
	static UWorld* GSHordeGameWorld(UWorld* Fallback)
	{
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
				{
					return Ctx.World();
				}
			}
		}
		return Fallback;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSHordeSpawnTestCmd(
	TEXT("GS.Horde.SpawnTest"),
	TEXT("GS.Horde.SpawnTest [n] - blow the horn n times (default 3) and report the pool. Use n=3 "
		 "to reach the ten-goblin exit test at 4 per blast."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSHordeGameWorld(InWorld);
			UGSHordeSubsystem* Horde = World ? World->GetSubsystem<UGSHordeSubsystem>() : nullptr;
			if (!Horde)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GoblinSiege] GS.Horde.SpawnTest: no UGSHordeSubsystem - are you in PIE?"));
				return;
			}

			const int32 Blasts = (Args.Num() > 0) ? FMath::Max(1, FCString::Atoi(*Args[0])) : 3;
			AController* Summoner = UGameplayStatics::GetPlayerController(World, 0);

			int32 Total = 0;
			for (int32 i = 0; i < Blasts; ++i)
			{
				Total += Horde->SummonWave(Summoner);
			}

			const FString Message = FString::Printf(
				TEXT("GS.Horde.SpawnTest: %d blast(s) summoned %d. Active %d/%d, reserve %d/%d."),
				Blasts, Total, Horde->GetActiveCount(), Horde->GetActiveCap(),
				Horde->GetReserveRemaining(), Horde->GetRaidPoolSize());

			UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] %s"), *Message);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green, Message);
			}
		}));

static FAutoConsoleCommandWithWorld GSHordeStatusCmd(
	TEXT("GS.Horde.Status"),
	TEXT("Print the horde pool: reserve remaining, active count, cap."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* InWorld)
		{
			UWorld* World = GSHordeGameWorld(InWorld);
			UGSHordeSubsystem* Horde = World ? World->GetSubsystem<UGSHordeSubsystem>() : nullptr;
			if (!Horde)
			{
				UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] GS.Horde.Status: no subsystem."));
				return;
			}

			const FString Message = FString::Printf(
				TEXT("Horde: reserve %d/%d, active %d/%d%s"),
				Horde->GetReserveRemaining(), Horde->GetRaidPoolSize(),
				Horde->GetActiveCount(), Horde->GetActiveCap(),
				Horde->IsPoolDry() ? TEXT(" - THE TREELINE IS SILENT") : TEXT(""));

			UE_LOG(LogTemp, Warning, TEXT("[GoblinSiege] %s"), *Message);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green, Message);
			}
		}));

// ------------------------------------------------------ GS.Combat.LogAI + GS.Combat.Duel (#083)
//
// GS.Combat.LogDamage answers "what happened to that hit". It cannot answer the question that
// actually costs the evenings once AI fight each other: "why did he not block?" - which has at
// least six causes that are indistinguishable from outside (no target, out of range, on cooldown,
// the roll failed, the telegraph was never seen, StartBlocking refused because the guard was
// already broken). Declared in AI/GSAIDebug.h so the three deciding TUs share one switch.

#include "AI/GSAIDebug.h"
#include "Characters/GSEnemyCharacter.h"
#include "Combat/GSRaceDataAsset.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSAI, Log, All);

static int32 GSCombatLogAI = 0;
static FAutoConsoleVariableRef CVarGSCombatLogAI(
	TEXT("GS.Combat.LogAI"),
	GSCombatLogAI,
	TEXT("0 = off, 1 = log every AI target / block / guard-break decision incl. refusals."),
	ECVF_Cheat);

namespace GSAIDebug
{
	bool IsLogging()
	{
		return GSCombatLogAI > 0;
	}

	void Log(const AActor* Who, const FString& Message)
	{
		if (GSCombatLogAI <= 0)
		{
			return;
		}
		UE_LOG(LogGSAI, Log, TEXT("[GS.AI] %s: %s"), *GetNameSafe(Who), *Message);
	}
}

// GS.Combat.Duel - stand up an NPC-vs-NPC fight on demand.
//
// There was no way to do this at all. GS.Raid.SpawnAt only redirects where the PLAYER spawns, and
// the placed defenders on every map are one faction, so "two AI fighting" was a state the game
// could not be put into. Every number in UBTTask_Block is a probability, and a probability cannot
// be tuned from a sample of one fight that took ten minutes to arrange by hand.
//
// The trick that makes this need no new content: AGSEnemyCharacter::InitializeFromArchetype is
// public and sets RaceTag from its data asset, so side B is an ordinary human defender Blueprint
// re-badged onto DA_Race_Goblin AFTER it spawns. It keeps its mesh, its abilities and the
// BT_Militia its controller already possessed it with - only its allegiance changes. No goblin
// pawn, animation or behaviour tree needs to exist for the melee to be exercised end to end.
//
// Every value is an argument with a default, so re-pointing the test at other archetypes never
// costs a six-minute rebuild.
static FAutoConsoleCommandWithWorldAndArgs GSCombatDuelCmd(
	TEXT("GS.Combat.Duel"),
	TEXT("GS.Combat.Duel [n=1] [classA] [classB] [raceBAsset] [raceBRow] - spawn n vs n in front of "
		 "the player and set side B to a hostile race so they fight each other. Defaults: Knight vs "
		 "CastleGuard01 re-badged onto DA_Race_Goblin/Brawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSHordeGameWorld(InWorld);
			if (!World)
			{
				UE_LOG(LogGSAI, Warning, TEXT("[GS.AI] GS.Combat.Duel: no game world - are you in PIE?"));
				return;
			}

			const int32 PerSide = (Args.Num() > 0) ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 16) : 1;
			const FString ClassAPath = (Args.Num() > 1) ? Args[1]
				: TEXT("/Game/Blueprints/Adversaries/BP_KnightDPelegrini.BP_KnightDPelegrini_C");
			const FString ClassBPath = (Args.Num() > 2) ? Args[2]
				: TEXT("/Game/Blueprints/Adversaries/BP_CastleGuard01.BP_CastleGuard01_C");
			const FString RaceBPath = (Args.Num() > 3) ? Args[3]
				: TEXT("/Game/AI/DA_Race_Goblin.DA_Race_Goblin");
			const FName RaceBRow = (Args.Num() > 4) ? FName(*Args[4]) : FName(TEXT("Brawler"));

			UClass* ClassA = LoadClass<AGSEnemyCharacter>(nullptr, *ClassAPath);
			UClass* ClassB = LoadClass<AGSEnemyCharacter>(nullptr, *ClassBPath);
			if (!ClassA || !ClassB)
			{
				// Naming the one that failed matters: the _C suffix is the usual mistake here and
				// "could not load" without a path sends you looking at the spawn code instead.
				UE_LOG(LogGSAI, Warning, TEXT("[GS.AI] GS.Combat.Duel: could not load %s"),
					!ClassA ? *ClassAPath : *ClassBPath);
				return;
			}

			UGSRaceDataAsset* RaceB = LoadObject<UGSRaceDataAsset>(nullptr, *RaceBPath);
			if (!RaceB)
			{
				UE_LOG(LogGSAI, Warning,
					TEXT("[GS.AI] GS.Combat.Duel: could not load race asset %s - both sides would be "
						 "the same race and would refuse to hit each other."), *RaceBPath);
				return;
			}

			// Centre the arena in front of the player so the fight is on screen when it starts.
			FVector Centre = FVector::ZeroVector;
			FVector Forward = FVector::ForwardVector;
			if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
			{
				Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
				Centre = PlayerPawn->GetActorLocation() + Forward * 900.f;
			}
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

			// Ground-snap each spawn individually. A flat arena makes this look pointless; the moment
			// this is run on L_Tutorial_Island it is the difference between a duel and two defenders
			// falling through the world or spawning inside a hillside.
			// Stand them ON the floor, using each class's OWN capsule. The first version added a flat
			// 95uu, which is shorter than these capsules: every combatant spawned intersecting the
			// floor and was shoved upwards by AdjustIfPossibleButAlwaysSpawn to a height that bore no
			// relation to the ground (observed 174-180 against a floor at 0). A small margin on top
			// so they settle down onto it rather than starting fractionally embedded.
			auto HalfHeightOf = [](UClass* CharClass) -> float
			{
				if (const ACharacter* CDO = CharClass ? CharClass->GetDefaultObject<ACharacter>() : nullptr)
				{
					if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
					{
						return Capsule->GetScaledCapsuleHalfHeight();
					}
				}
				return 95.f;
			};
			const float HalfA = HalfHeightOf(ClassA);
			const float HalfB = HalfHeightOf(ClassB);

			auto GroundSnap = [World](const FVector& In, float HalfHeight) -> FVector
			{
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GSCombatDuelGroundSnap), false);
				if (World->LineTraceSingleByChannel(Hit, In + FVector(0, 0, 500.f),
						In - FVector(0, 0, 2000.f), ECC_WorldStatic, Params))
				{
					return Hit.ImpactPoint + FVector(0, 0, HalfHeight + 4.f);
				}
				return In;
			};

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			int32 SpawnedA = 0;
			int32 SpawnedB = 0;
			for (int32 i = 0; i < PerSide; ++i)
			{
				// Fan sideways so n-vs-n starts as a line rather than a single stack of capsules.
				const FVector Lateral = Right * ((static_cast<float>(i) - (PerSide - 1) * 0.5f) * 200.f);

				const FVector LocA = GroundSnap(Centre - Forward * 300.f + Lateral, HalfA);
				const FVector LocB = GroundSnap(Centre + Forward * 300.f + Lateral, HalfB);

				if (AGSEnemyCharacter* A = World->SpawnActor<AGSEnemyCharacter>(
						ClassA, LocA, Forward.Rotation(), SpawnParams))
				{
					++SpawnedA;
					(void)A;
				}

				if (AGSEnemyCharacter* B = World->SpawnActor<AGSEnemyCharacter>(
						ClassB, LocB, (-Forward).Rotation(), SpawnParams))
				{
					// The whole point of the command. Re-badges an ordinary human defender onto the
					// goblin race so IsHostileTo returns true in both directions and the melee sweep,
					// the block arc and the guard break all start applying between two AI.
					B->InitializeFromArchetype(RaceB, RaceBRow);
					++SpawnedB;
				}
			}

			const FString Message = FString::Printf(
				TEXT("GS.Combat.Duel: %d x %s vs %d x %s (%s/%s). Turn on GS.Combat.LogDamage 1 and "
					 "GS.Combat.LogAI 1."),
				SpawnedA, *ClassA->GetName(), SpawnedB, *ClassB->GetName(),
				*RaceB->GetName(), *RaceBRow.ToString());

			UE_LOG(LogGSAI, Warning, TEXT("[GS.AI] %s"), *Message);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, Message);
			}
		}));

// ------------------------------------------------------------------ GS.Combat.SpawnPatrol (#110)
//
// Reinforce the HUMAN side on demand. GS.Combat.Duel exists but is a different tool: it spawns two
// sides and re-badges one onto a hostile race so AI fight AI. This one adds ordinary defenders, who
// are already hostile to a goblin player, so the fight is the one the raid actually has.
//
// Defaults are Michael's squad ruling: "a typical squad you would fight in a city like this is 3-4
// militia swordsmen and 1-2 archers". Knights default to zero because they are elites - "at most 3-4
// on the entire map at this difficulty".
//
// Distance defaults to 2000uu, comfortably outside BTService_AcquireTarget's AcquireRadius of 1500,
// so a squad can be placed and looked at before it notices anyone. That is the point of the command:
// stack up a force, then walk into it.

static FAutoConsoleCommandWithWorldAndArgs GSCmdSpawnPatrol(
	TEXT("GS.Combat.SpawnPatrol"),
	TEXT("GS.Combat.SpawnPatrol [militia=4] [archers=2] [knights=0] [distance=2000] - spawn a human "
		 "patrol in front of the player. They are hostile to a goblin on race alone, so no re-badging. "
		 "Run it repeatedly to mass a bigger force."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSHordeGameWorld(InWorld);
			if (!World)
			{
				UE_LOG(LogGSAI, Warning,
					TEXT("[GS.AI] GS.Combat.SpawnPatrol: no game world - are you in PIE or standalone?"));
				return;
			}

			const int32 NumMilitia = (Args.Num() > 0) ? FMath::Clamp(FCString::Atoi(*Args[0]), 0, 24) : 4;
			const int32 NumArchers = (Args.Num() > 1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 0, 12) : 2;
			const int32 NumKnights = (Args.Num() > 2) ? FMath::Clamp(FCString::Atoi(*Args[2]), 0, 8)  : 0;
			const float Distance   = (Args.Num() > 3) ? FMath::Clamp(FCString::Atof(*Args[3]), 300.f, 8000.f) : 2000.f;

			// Two militia Blueprints alternating, so a line of swordsmen is not six of the same man.
			UClass* MilitiaA = LoadClass<AGSEnemyCharacter>(nullptr,
				TEXT("/Game/Blueprints/Adversaries/BP_CastleGuard01.BP_CastleGuard01_C"));
			UClass* MilitiaB = LoadClass<AGSEnemyCharacter>(nullptr,
				TEXT("/Game/Blueprints/Adversaries/BP_CastleGuard02.BP_CastleGuard02_C"));
			UClass* Archer = LoadClass<AGSEnemyCharacter>(nullptr,
				TEXT("/Game/Blueprints/Adversaries/BP_ErikaArcher.BP_ErikaArcher_C"));
			UClass* Knight = LoadClass<AGSEnemyCharacter>(nullptr,
				TEXT("/Game/Blueprints/Adversaries/BP_KnightDPelegrini.BP_KnightDPelegrini_C"));

			if (!MilitiaA || !Archer)
			{
				UE_LOG(LogGSAI, Warning,
					TEXT("[GS.AI] GS.Combat.SpawnPatrol: could not load the defender Blueprints."));
				return;
			}
			if (!MilitiaB) { MilitiaB = MilitiaA; }

			FVector Centre = FVector::ZeroVector;
			FVector Forward = FVector::ForwardVector;
			if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
			{
				Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
				Centre = PlayerPawn->GetActorLocation() + Forward * Distance;
			}
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

			// Same ground-snap reasoning as GS.Combat.Duel: use each class's OWN capsule, because a
			// flat additive height spawns them embedded in the floor and the spawn handler then shoves
			// them to an arbitrary height.
			auto HalfHeightOf = [](UClass* CharClass) -> float
			{
				if (const ACharacter* CDO = CharClass ? CharClass->GetDefaultObject<ACharacter>() : nullptr)
				{
					if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
					{
						return Capsule->GetScaledCapsuleHalfHeight();
					}
				}
				return 95.f;
			};
			auto GroundSnap = [World](const FVector& In, float HalfHeight) -> FVector
			{
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GSSpawnPatrolGroundSnap), false);
				if (World->LineTraceSingleByChannel(Hit, In + FVector(0, 0, 500.f),
						In - FVector(0, 0, 2000.f), ECC_WorldStatic, Params))
				{
					return Hit.ImpactPoint + FVector(0, 0, HalfHeight + 4.f);
				}
				return In;
			};

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			int32 Spawned = 0;
			auto SpawnRank = [&](UClass* ClassEven, UClass* ClassOdd, int32 Count, float RankOffset)
			{
				const float Half = HalfHeightOf(ClassEven);
				for (int32 i = 0; i < Count; ++i)
				{
					// Spread on the SAME 200uu pitch the duel uses, which clears even a 70uu guard
					// capsule with room to walk out of.
					const FVector Lateral = Right * ((static_cast<float>(i) - (Count - 1) * 0.5f) * 200.f);
					const FVector Loc = GroundSnap(Centre + Forward * RankOffset + Lateral, Half);
					UClass* Pick = ((i % 2) == 0) ? ClassEven : ClassOdd;
					if (World->SpawnActor<AGSEnemyCharacter>(Pick, Loc, (-Forward).Rotation(), SpawnParams))
					{
						++Spawned;
					}
				}
			};

			// Archers behind the line, as they would stand. No re-badging anywhere: a human defender
			// is already hostile to a goblin through RaceTag.
			SpawnRank(MilitiaA, MilitiaB, NumMilitia, 0.f);
			SpawnRank(Archer, Archer, NumArchers, 260.f);
			if (NumKnights > 0 && Knight)
			{
				SpawnRank(Knight, Knight, NumKnights, -160.f);
			}

			const FString Message = FString::Printf(
				TEXT("GS.Combat.SpawnPatrol: %d defenders at %.0fuu (%d militia, %d archers, %d knights)."),
				Spawned, Distance, NumMilitia, NumArchers, NumKnights);
			UE_LOG(LogGSAI, Warning, TEXT("[GS.AI] %s"), *Message);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, Message);
			}
		}));

// ------------------------------------------------------------------- GS.Combat.CrowdStats (#090)
//
// Every number in the massed-combat pass is a CAP: token weight per victim, attackers per victim,
// ring slots per victim. A cap you cannot observe is a cap you cannot trust - and the failure mode
// is silent in both directions. Too loose and the fight is a blender; too tight and half the
// warband stands in the menace ring forever, which reads as broken AI rather than as a queue.
//
// Prints one row per actor that currently has anyone engaged on it, so a 10v10 is a dozen lines
// rather than a wall.

#include "Combat/GSEngagementComponent.h"
#include "EngineUtils.h"

static FAutoConsoleCommandWithWorld GSCombatCrowdStatsCmd(
	TEXT("GS.Combat.CrowdStats"),
	TEXT("Per-victim engagement: attackers assigned, attack weight reserved vs budget, ring slots "
		 "claimed. Use during a crowd fight to check the caps are holding."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* InWorld)
		{
			UWorld* World = GSHordeGameWorld(InWorld);
			if (!World)
			{
				UE_LOG(LogGSAI, Warning, TEXT("[GS.AI] GS.Combat.CrowdStats: no game world."));
				return;
			}

			int32 Rows = 0;
			int32 WorstGang = 0;
			FString WorstName;

			for (TActorIterator<AGSCharacterBase> It(World); It; ++It)
			{
				AGSCharacterBase* Victim = *It;
				if (!IsValid(Victim))
				{
					continue;
				}
				const UGSEngagementComponent* Engagement = Victim->GetEngagement();
				if (!Engagement)
				{
					continue;
				}

				const int32 Engaged = Engagement->GetEngagedCount();
				if (Engaged == 0)
				{
					continue;   // quiet actors are noise in a crowd readout
				}

				const int32 Attackers = Engagement->GetAttackerCount();
				const int32 Weight = Engagement->GetReservedWeight();
				const int32 Budget = Engagement->GetTokenBudget();

				if (Engaged > WorstGang)
				{
					WorstGang = Engaged;
					WorstName = Victim->GetName();
				}

				// The OVER markers are the point: they are the only thing in this readout that says
				// a cap has actually been breached rather than merely approached.
				UE_LOG(LogGSAI, Warning,
					TEXT("[GS.Crowd] %-28s engaged %d  swinging %d  weight %d/%d%s  slots %d  %s"),
					*Victim->GetName(), Engaged, Attackers, Weight, Budget,
					(Weight > Budget) ? TEXT(" <-- OVER BUDGET") : TEXT(""),
					Engagement->GetClaimedSlotCount(),
					Engagement->CanBeAttacked() ? TEXT("") : TEXT("(immune: staggered/recoiling/dead)"));
				++Rows;
			}

			const FString Summary = (Rows == 0)
				? FString(TEXT("GS.Combat.CrowdStats: nobody is engaged with anybody."))
				: FString::Printf(TEXT("GS.Combat.CrowdStats: %d victim(s) engaged, biggest gang %d on %s."),
					Rows, WorstGang, *WorstName);

			UE_LOG(LogGSAI, Warning, TEXT("[GS.Crowd] %s"), *Summary);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Summary);
			}
		}));
