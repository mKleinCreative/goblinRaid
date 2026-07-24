/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadBuildPresetFactory.h"
#include "Assets/RoadBuildPreset.h"
#include "RoadMeshBuild/MetaRoadBuildSettings.h"

URoadBuildPresetFactory::URoadBuildPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = URoadBuildPreset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = false;
}

UObject* URoadBuildPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	URoadBuildPreset* Preset = NewObject<URoadBuildPreset>(InParent, Name, Flags);
	// Default-initialized settings; callers (the preset panel "Save As") overwrite Settings with a copy
	// of the actor's current build settings right after creation.
	Preset->Settings = NewObject<UMetaRoadBuildSettings>(Preset);
	Preset->Settings->EnsureDefaultPropertySets();
	return Preset;
}

bool URoadBuildPresetFactory::CanCreateNew() const
{
	return true;
}
