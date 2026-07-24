/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadFbxExportSettings.h"

#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadFbxExportSettings)

UMetaRoadFbxExportSettings::UMetaRoadFbxExportSettings()
{
	// Default to <Project>/Saved/Export/FBX. Stored as an absolute path so it survives regardless of CWD.
	// (Field defaults other than this are in-class initializers, see the header.)
	if (OutputDirectory.Path.IsEmpty())
	{
		OutputDirectory.Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Export") / TEXT("FBX"));
	}
}

#if WITH_EDITOR
void UMetaRoadFbxExportSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Persist edits (the panel edits the CDO).
	SaveConfig();
}
#endif
