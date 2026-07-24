/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMarkProfileFactory.h"
#include "Assets/RoadMarkProfile.h"

URoadMarkProfileFactory::URoadMarkProfileFactory(const FObjectInitializer& objectInitializer) 
	: Super(objectInitializer) 
{
	SupportedClass = URoadMarkProfile::StaticClass();
}

UObject* URoadMarkProfileFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    URoadMarkProfile* Asset = NewObject<URoadMarkProfile>(InParent, Name, Flags);
	return Asset;
}

bool URoadMarkProfileFactory::CanCreateNew() const 
{
    return true;
}

FName URoadMarkProfileFactory::GetNewAssetIconOverride() const
{
	return "RoadEditor.RoadLaneMarkMode";
}