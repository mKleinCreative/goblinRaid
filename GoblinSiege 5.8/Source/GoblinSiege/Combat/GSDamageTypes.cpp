// Copyright Goblin Siege.

#include "Combat/GSDamageTypes.h"
#include "Combat/GSGameplayTags.h"

UGSDamageType_Axe::UGSDamageType_Axe()
{
	// Damage.Dagger is the existing melee tag on the player's weapon path - the mesh became an axe
	// (SM_WoodcutterAxe) but the tag did not, and renaming it would touch every ability and
	// projectile that already references it. The class name says axe; the tag stays what the rest of
	// the project already agrees on.
	DamageTags.AddTag(GSTags::Damage_Dagger);
}

UGSDamageType_Bow::UGSDamageType_Bow()
{
	DamageTags.AddTag(GSTags::Damage_Bow);
}

UGSDamageType_Fire::UGSDamageType_Fire()
{
	DamageTags.AddTag(GSTags::Damage_Fire);

	// Fire ignores immortality for the same reason it ignores race: a burning field is not something
	// anyone is exempt from.
	bAffectedByImmortality = false;
}
