#include "Combat/GSDamageExecCalculation.h"
#include "Combat/GSGameplayTags.h"
#include "Combat/GSRaceDataAsset.h"
#include "Attributes/GSAttributeSetBase.h"
#include "Characters/GSCharacterBase.h"
#include "Characters/GSEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

// 2026-08-02: combat damage was completely silent, and this function has TWO paths that return
// without applying anything. On screen, "the trace missed", "the target has no ASC", "no Damage.*
// tag on the spec" and "armor ate it" are four different bugs that look identical - each costs an
// hour to tell apart by guessing. One cvar makes them one glance.
//   GS.Combat.LogDamage 1   -> log line per resolution
//   GS.Combat.LogDamage 2   -> also print on screen
static int32 GSCombatLogDamage = 0;
static FAutoConsoleVariableRef CVarGSCombatLogDamage(
	TEXT("GS.Combat.LogDamage"),
	GSCombatLogDamage,
	TEXT("0 = off, 1 = log every damage resolution incl. rejections, 2 = also print on screen."),
	ECVF_Cheat);

// Block tuning, cvars rather than data assets on purpose: these are two numbers that decide
// whether blocking feels worth doing, and they want to be draggable during a playtest without a
// rebuild or an asset edit. They move onto the weapon/shield data asset once the feel is settled.
static float GSBlockDamageMultiplier = 0.2f;
static FAutoConsoleVariableRef CVarGSBlockMult(
	TEXT("GS.Combat.BlockMultiplier"),
	GSBlockDamageMultiplier,
	TEXT("Damage multiplier applied to a successful frontal block, before armor. 0 = perfect block."),
	ECVF_Cheat);

static float GSBlockArcDegrees = 140.f;
static FAutoConsoleVariableRef CVarGSBlockArc(
	TEXT("GS.Combat.BlockArc"),
	GSBlockArcDegrees,
	TEXT("Total frontal arc, in degrees, within which a block applies."),
	ECVF_Cheat);

// A blocked swing is TURNED ASIDE, not merely reduced (Michael's ruling 2026-08-08). The attacker's
// swing is cancelled and they are left open for this long - no attacking, no re-guarding.
//
// This is the number that decides whether blocking is a decision or a formality. At 0 a block is
// just "take 20% instead of 100%", two competent guards deadlock, and an allied goblin - which
// carries no guard break - has no way through a defender who turtles. Too high and one read ends
// the fight. 0.6s is roughly a full swing cycle at current windup+window+recovery, so a punish
// lands but a second one does not.
// NPC-vs-NPC is its own damage channel, tuned to a time-to-kill budget rather than to the player's
// weapon numbers. The research is unanimous that ally/enemy lethality must be throttled separately:
// The Last of Us re-tunes Ellie's accuracy and fire rate per encounter and gives her shots outside
// the player's view no damage at all.
//
// The number to hit is 10-18s over 5-8 landed hits. At the player's numbers a goblin-vs-militiaman
// duel is four hits and ~7s, which is over before a spectator can read who is winning.
// Directional plate (GDD §217). Only ever applies to a character with Armor > 0, so these two are
// knight dials and nothing else's - a militiaman at armour 0 never touches this path.
static float GSPlateArcDegrees = 150.f;
static FAutoConsoleVariableRef CVarGSPlateArc(
	TEXT("GS.Combat.PlateArc"),
	GSPlateArcDegrees,
	TEXT("Total frontal arc, in degrees, within which plate is at its best. Wider than the 140 block "
		 "arc on purpose: a shield is aimed, a breastplate simply faces where the man faces."),
	ECVF_Cheat);

// 0.3 -> 0.6 (Michael's ruling, #345). At 0.3 a player light attack on a knight's front was
// 25 x 0.3 - 6 = 1.5 against 75 health: FIFTY swings to kill him head-on, where a flank skips the
// plate entirely and does it in three. The counter was right and the number made it academic -
// nobody flanks because the front is expensive, they flank because the front is impossible, and a
// health bar that will not move reads as a broken hit rather than as armour. 0.6 gives 25 x 0.6 - 6
// = 9, so roughly nine frontal swings against three from the side: flanking stays clearly correct
// and the front stays clearly worse, which is what the design actually asks for.
//
// This is a FEEL number, not a balance derivation - it is a cvar so it can be dialled live.
static float GSPlateFrontalScalar = 0.6f;
static FAutoConsoleVariableRef CVarGSPlateFrontal(
	TEXT("GS.Combat.PlateFrontalScalar"),
	GSPlateFrontalScalar,
	TEXT("Damage multiplier for a frontal hit on an armoured target, applied BEFORE flat armour. "
		 "0.6 makes a 25-damage swing land for ~9 on a knight; 0.3 made it ~1.5, which read as a "
		 "hit that did nothing at all."),
	ECVF_Cheat);

// MITIGATION MAY NEVER ZERO A HIT (Michael's ruling 2026-08-09).
//
// Every mitigation in this function multiplies or subtracts, and they compound: a goblin's frontal
// swing on a knight was 25 x 0.55 (NPC-vs-NPC) x 0.3 (plate) - 6 (armour) = exactly 0.00, so the
// warband could not scratch him from the front at all. "Immune" and "heavily resistant" are
// different games; a connected blow should always take something off.
//
// It is a FLOOR, not a bonus - it only ever raises a hit that mitigation drove below it, so the
// tuned numbers above are untouched wherever they already produce a real result. It also applies to
// blocks (chip damage through a raised guard) and to fire, where it stops 6 points of armour
// quietly making a knight fireproof against the goblins' main equaliser.
static float GSMinimumDamage = 1.f;
static FAutoConsoleVariableRef CVarGSMinimumDamage(
	TEXT("GS.Combat.MinimumDamage"),
	GSMinimumDamage,
	TEXT("Least damage any connected hit may do after all mitigation. 0 restores the old behaviour "
		 "where armour and plate could reduce a blow to nothing."),
	ECVF_Cheat);

static float GSNPCvNPCDamageScalar = 0.55f;
static FAutoConsoleVariableRef CVarGSNPCvNPCScalar(
	TEXT("GS.Combat.NPCvNPCScalar"),
	GSNPCvNPCDamageScalar,
	TEXT("Damage multiplier when NEITHER party is player-controlled. 1 = same as player damage."),
	ECVF_Cheat);

static float GSRecoilSeconds = 0.6f;
static FAutoConsoleVariableRef CVarGSRecoilSeconds(
	TEXT("GS.Combat.RecoilSeconds"),
	GSRecoilSeconds,
	TEXT("How long an attacker is left open after their swing is blocked. 0 disables the punish."),
	ECVF_Cheat);

static void GSLogDamage(const FString& Msg, bool bIsRejection)
{
	if (GSCombatLogDamage <= 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[GS.Damage] %s"), *Msg);

	if (GSCombatLogDamage >= 2 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f,
			bIsRejection ? FColor::Orange : FColor::Yellow, FString::Printf(TEXT("[dmg] %s"), *Msg));
	}
}

/** SetByCaller tag the attacking GameplayAbility must set: EffectSpec.SetSetByCallerMagnitude(
 *  GSTags::Damage_Dagger (or Bow/Greatclub/ShadowMagic/BloodMagic/Fire/Blast), RawDamageValue). */
static const FGameplayTag& GetRawDamageSetByCallerTagForType(const FGameplayTagContainer& AssetTags)
{
	// The attacking ability tags its GameplayEffectSpec with exactly one Damage.* asset tag; that
	// same tag doubles as the SetByCaller key, so the exec calc only needs to know the enum of
	// possible types, not a separate lookup table.
	static const TArray<FGameplayTag> DamageTypeTags = {
		GSTags::Damage_Dagger, GSTags::Damage_Bow, GSTags::Damage_Greatclub,
		GSTags::Damage_ShadowMagic, GSTags::Damage_BloodMagic, GSTags::Damage_Fire, GSTags::Damage_Blast
	};

	for (const FGameplayTag& Tag : DamageTypeTags)
	{
		if (AssetTags.HasTagExact(Tag))
		{
			return Tag;
		}
	}
	return FGameplayTag::EmptyTag;
}

UGSDamageExecCalculation::UGSDamageExecCalculation()
{
	// No FGameplayEffectAttributeCaptureDefinition members are declared for Armor here because we
	// read it directly off the target's AttributeSetBase in Execute_Implementation instead of via
	// the capture-definition indirection - keeps this scaffold readable. A production
	// implementation should add a proper FProperty-based capture definition for Armor (and
	// snapshot=false so late armor buffs/debuffs are respected) instead of the direct read below.
}

void UGSDamageExecCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	// The attacking ability calls Spec.AddDynamicAssetTag(GSTags::Damage_Fire) (etc.) before
	// commit, so exactly one Damage.* tag lives here alongside SetByCaller magnitude - simpler and
	// more idiomatic than reading captured source tags for this scaffold's purposes.
	const FGameplayTagContainer& AssetTags = Spec.GetDynamicAssetTags();

	const FGameplayTag DamageTypeTag = GetRawDamageSetByCallerTagForType(AssetTags);
	if (!DamageTypeTag.IsValid())
	{
		// Misconfigured attacking ability. By far the most likely cause is a Blueprint GE child
		// carrying Damage.* in its ASSET tags instead of the ability calling AddAssetTag on the
		// spec handle - the two look identical in the editor and only this path can tell you apart.
		GSLogDamage(FString::Printf(TEXT("REJECTED: no Damage.* tag on spec (dynamic tags: %s)"),
			*AssetTags.ToStringSimple()), true);
		return;
	}

	const float RawDamage = Spec.GetSetByCallerMagnitude(DamageTypeTag, false, 0.f);
	if (RawDamage <= 0.f)
	{
		GSLogDamage(FString::Printf(TEXT("REJECTED: %s tagged but SetByCaller magnitude is %.2f"),
			*DamageTypeTag.GetTagName().ToString(), RawDamage), true);
		return;
	}

	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	// Race matchup multiplier (design doc §5 / every race-design-*.md table).
	float RaceMultiplier = 1.f;
	if (const AGSEnemyCharacter* EnemyTarget = Cast<AGSEnemyCharacter>(TargetActor))
	{
		if (const UGSRaceDataAsset* RaceData = EnemyTarget->GetRaceData())
		{
			RaceMultiplier = RaceData->GetDamageMultiplier(DamageTypeTag);
		}
	}

	float DamageAfterRace = RawDamage * RaceMultiplier;

	// Neither side player-controlled? Then this is a fight the player is WATCHING, and it should
	// last long enough to be read. Tested on the pawn's controller rather than a class cast so a
	// possessed or debug-driven pawn is treated as the player automatically.
	{
		const AActor* Attacker = Spec.GetContext().GetInstigator();
		const APawn* AttackerPawn = Cast<APawn>(Attacker);
		const APawn* VictimPawn = Cast<APawn>(TargetActor);
		const bool bAttackerIsPlayer = AttackerPawn && AttackerPawn->IsPlayerControlled();
		const bool bVictimIsPlayer = VictimPawn && VictimPawn->IsPlayerControlled();

		if (!bAttackerIsPlayer && !bVictimIsPlayer)
		{
			DamageAfterRace *= GSNPCvNPCDamageScalar;
		}
	}

	// Armor mitigation - skipped for Blast (barrels) and anything explicitly flagged
	// Damage.IgnoresArmor (Shadow/Blood magic), per every race doc's "explosives/curses bypass
	// armor" callouts.
	const bool bSkipArmor = DamageTypeTag == GSTags::Damage_Blast || AssetTags.HasTagExact(GSTags::Damage_IgnoresArmor);

	// ---- block (2026-08-03) -------------------------------------------------------------
	// Deliberately BEFORE armor: a guard is the outer layer. Frontal only - a block that
	// protects your back is not a block, and being able to turtle in every direction removes
	// the only decision blocking asks you to make.
	bool bBlocked = false;
	if (TargetASC && TargetASC->HasMatchingGameplayTag(GSTags::State_Blocking) && TargetActor)
	{
		const AActor* Attacker = Spec.GetContext().GetInstigator();
		if (Attacker)
		{
			FVector ToAttacker = Attacker->GetActorLocation() - TargetActor->GetActorLocation();
			ToAttacker.Z = 0.f;
			if (!ToAttacker.IsNearlyZero())
			{
				const float Facing = FVector::DotProduct(ToAttacker.GetSafeNormal(),
					TargetActor->GetActorForwardVector());
				bBlocked = Facing >= FMath::Cos(FMath::DegreesToRadians(GSBlockArcDegrees * 0.5f));
			}
		}
		else
		{
			// No instigator (environmental damage, e.g. standing in fire). A raised shield does
			// nothing against the floor being on fire, so this stays unblocked on purpose.
			bBlocked = false;
		}
	}

	if (bBlocked)
	{
		DamageAfterRace *= GSBlockDamageMultiplier;

		// The guard did not just absorb the hit - it turned the swing aside. Cancel it and leave the
		// attacker open. Done here rather than in the ability because this is the ONE place that
		// knows a block actually resolved: the ability knows it swung, and the victim knows it was
		// guarding, but only this calculation knows the arc test passed.
		if (AGSCharacterBase* AttackerCharacter = Cast<AGSCharacterBase>(Spec.GetContext().GetInstigator()))
		{
			AttackerCharacter->NotifyAttackWasBlocked(TargetActor, GSRecoilSeconds);
		}
	}

	// ---- directional plate (2026-08-09, GDD §217) -----------------------------------------
	// "knights whose plate shrugs off a straight-on dagger rush but not a takedown from the shadows
	// or a bow shot placed at the gaps."
	//
	// Flat armour could not express that: 6 points mitigated a blade in the back exactly as well as
	// one on the breastplate, so a knight was only a militiaman with more health and the counters
	// the design names - flank him, shoot him, take him from behind - bought you nothing.
	//
	// Now armour is POSITIONAL, and it costs no new data: it keys off the armour value that already
	// distinguishes a knight (6) from a militiaman (0), so an unarmoured defender is untouched by
	// every line below. Reuses the same flat-dot facing test as the block arc - one idea about what
	// "in front of me" means, not two.
	float FinalDamage = DamageAfterRace;
	bool bStraightOn = false;   // function scope: the damage log reports which face of the plate was hit
	if (!bSkipArmor)
	{
		if (const UGSAttributeSetBase* TargetAttributes = TargetASC
				? TargetASC->GetSet<UGSAttributeSetBase>()
				: nullptr)
		{
			const float Armor = TargetAttributes->GetArmor();

			// A bow finds the gaps. This is the design's own named counter, so it is absolute rather
			// than a modifier - no arc test, no armour, at any angle. The same is true for whoever
			// eventually gives allied archers a bow: consistency is the point.
			const bool bFindsGaps = (DamageTypeTag == GSTags::Damage_Bow);

			if (Armor > 0.f && !bFindsGaps && TargetActor)
			{
				if (const AActor* Attacker = Spec.GetContext().GetInstigator())
				{
					FVector ToAttacker = Attacker->GetActorLocation() - TargetActor->GetActorLocation();
					ToAttacker.Z = 0.f;
					if (!ToAttacker.IsNearlyZero())
					{
						const float Facing = FVector::DotProduct(ToAttacker.GetSafeNormal(),
							TargetActor->GetActorForwardVector());
						bStraightOn = Facing >= FMath::Cos(FMath::DegreesToRadians(GSPlateArcDegrees * 0.5f));
					}
				}
				else
				{
					// No instigator - fire, a falling beam. Plate helps against none of that, and
					// treating it as frontal would make a burning knight nearly fireproof.
					bStraightOn = false;
				}
			}

			if (bFindsGaps)
			{
				// Straight through. A knight's plate is not a health bar you chip at with arrows;
				// it is a thing you go around.
				FinalDamage = DamageAfterRace;
			}
			else if (bStraightOn)
			{
				// The dagger rush. Scalar FIRST, then the flat armour, so plate compounds against a
				// frontal attack instead of merely subtracting from it.
				FinalDamage = FMath::Max(0.f, DamageAfterRace * GSPlateFrontalScalar - Armor);

				// ---- TELL THE SWING IT WAS DEFLECTED (#355) --------------------------------------
				//
				// This is the whole reason Michael's knight read as a sponge: the plate did its job
				// and nothing said so. A frontal hit and a flank hit produced the identical impact,
				// so "go around him" was invisible - the player saw a health bar refusing to move
				// and concluded the hit had failed rather than been refused.
				//
				// A loose tag on the ATTACKER, not a callback into the ability: this calc is running
				// inside UGSGA_SwordLight::DoSweep's loop (see NotifyAttackWasBlocked for the same
				// hazard), so the swing reads the tag after ApplyGameplayEffectSpecToTarget returns
				// and clears it itself. Set-not-add, because it is a verdict, not a stack.
				if (UAbilitySystemComponent* AttackerASC =
						UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Spec.GetContext().GetInstigator()))
				{
					AttackerASC->SetLooseGameplayTagCount(GSTags::State_LastHitDeflected, 1);
				}
			}
			else if (Armor > 0.f)
			{
				// Flank, back, or a takedown from the shadows - the gaps in the harness. The flat
				// armour is skipped entirely rather than reduced: "not a takedown from the shadows"
				// is a statement about what WORKS, and a 25% discount would not read as working.
				FinalDamage = DamageAfterRace;
			}
			// No final else: the three branches above are exhaustive for Armor > 0, and every path
			// that reaches here with Armor <= 0 already holds FinalDamage = DamageAfterRace from the
			// initialiser. The else that used to sit here subtracted a non-positive Armor, i.e. it
			// would AMPLIFY a hit if an armour-sunder effect ever drove the attribute negative.
		}
	}

	// The floor, applied once at the very end so it catches every mitigation path - block, plate,
	// armour, race matchup, the NPC-vs-NPC scalar - rather than needing a guard in each. Gated on
	// RawDamage so it can only ever lift a real blow: an effect that was always going to do nothing
	// must not be promoted into doing one point.
	if (RawDamage > 0.f && GSMinimumDamage > 0.f)
	{
		FinalDamage = FMath::Max(FinalDamage, GSMinimumDamage);
	}

	if (GSCombatLogDamage > 0)
	{
		const UGSAttributeSetBase* Attrs = TargetASC ? TargetASC->GetSet<UGSAttributeSetBase>() : nullptr;
		// PlateOutcome is here because "why did that bounce?" is otherwise unanswerable from a log:
		// a 1.5-damage hit and a 25-damage hit differ only by an angle nobody can see after the fact.
		const float LoggedArmor = Attrs ? Attrs->GetArmor() : 0.f;
		const TCHAR* PlateOutcome = TEXT("");
		if (LoggedArmor > 0.f && !bSkipArmor)
		{
			// The facing dot is printed alongside the verdict (#355). "flank" on a blow the tester
			// swore was frontal cost three PIE runs to diagnose - the target had re-faced toward a
			// patrol waypoint inside the windup. With the number in the line, that reads instantly.
			PlateOutcome = (DamageTypeTag == GSTags::Damage_Bow) ? TEXT("PLATE-GAPS(bow) ")
				: (bStraightOn ? TEXT("PLATE-FRONT ") : TEXT("PLATE-GAPS(flank) "));
		}

		GSLogDamage(FString::Printf(
			TEXT("%s -> %s  %s  raw %.1f  x%.2f race  %s%s%s armor %.1f  = %.1f   (HP %.0f/%.0f)"),
			*GetNameSafe(Spec.GetContext().GetInstigator()),
			*GetNameSafe(TargetActor),
			*DamageTypeTag.GetTagName().ToString(),
			RawDamage, RaceMultiplier,
			bBlocked ? TEXT("BLOCKED ") : TEXT(""),
			PlateOutcome,
			bSkipArmor ? TEXT("SKIP") : TEXT("-"),
			Attrs ? Attrs->GetArmor() : 0.f,
			FinalDamage,
			Attrs ? Attrs->GetHealth() : 0.f,
			Attrs ? Attrs->GetMaxHealth() : 0.f),
			FinalDamage <= 0.f);
	}

	if (FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UGSAttributeSetBase::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}
}
