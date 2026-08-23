// Copyright Goblin Siege.

#pragma once

#include "CoreMinimal.h"
#include "Game/ACFDamageType.h"
#include "GSDamageTypes.generated.h"

/**
 * ACF identifies damage by CLASS (`FACFDamageEvent::DamageClass`), where this project has always
 * identified it by TAG (`Damage.Bow`, `Damage.Fire`, ...). These classes are the bridge: each is an
 * ACF damage type whose `DamageTags` container carries the tag we already use, so
 * UGSACFDamageCalculation can keep asking the question it has always asked - "is this a bow?" -
 * without a second taxonomy to keep in step.
 *
 * A parallel enum or a switch on class would have been the obvious thing and the wrong one: the tags
 * are referenced from abilities, projectiles and fire volumes already, and two sources of "what kind
 * of damage is this" is the failure this project keeps finding in ACF itself.
 *
 * Deriving from ACF's own UMeleeDamageType / URangedDamageType where they fit, so anything in ACF
 * that reasons about melee-vs-ranged still gets the right answer.
 */
UCLASS()
class GOBLINSIEGE_API UGSDamageType_Axe : public UMeleeDamageType
{
	GENERATED_BODY()
public:
	UGSDamageType_Axe();
};

/** The bow. Finds the gaps in plate: UGSACFDamageCalculation skips the frontal arc entirely for it. */
UCLASS()
class GOBLINSIEGE_API UGSDamageType_Bow : public URangedDamageType
{
	GENERATED_BODY()
public:
	UGSDamageType_Bow();
};

/** Torches and fire volumes. The goblin equalizer - burns everyone, including the goblin holding it,
 *  which is why the race check in the calculator deliberately does not apply to this. */
UCLASS()
class GOBLINSIEGE_API UGSDamageType_Fire : public UACFDamageType
{
	GENERATED_BODY()
public:
	UGSDamageType_Fire();
};
