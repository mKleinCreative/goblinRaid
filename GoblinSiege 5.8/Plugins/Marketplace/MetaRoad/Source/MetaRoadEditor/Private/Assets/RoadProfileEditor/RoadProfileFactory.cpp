/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadProfileFactory.h"
#include "Assets/RoadProfile.h"

URoadProfileFactory::URoadProfileFactory(const FObjectInitializer& objectInitializer) 
	: Super(objectInitializer) 
{
	SupportedClass = URoadProfile::StaticClass();
}

UObject* URoadProfileFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    URoadProfile* Asset = NewObject<URoadProfile>(InParent, Name, Flags);

    // Start a new profile with a minimal two-lane road: one default driving lane on each side.
    Asset->AddLane(MetaRoad::ZeroLaneIndex, /*bOnLeft=*/true);
    Asset->AddLane(MetaRoad::ZeroLaneIndex, /*bOnLeft=*/false);

	return Asset;
}

bool URoadProfileFactory::CanCreateNew() const 
{
    return true;
}
