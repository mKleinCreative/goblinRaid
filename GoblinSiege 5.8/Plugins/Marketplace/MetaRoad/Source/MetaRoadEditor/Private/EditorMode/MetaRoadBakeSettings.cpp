/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "EditorMode/MetaRoadBakeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadBakeSettings)

UMetaRoadBakeSettings::UMetaRoadBakeSettings()
{
	// Field defaults are set as in-class initializers (see the header).
}

#if WITH_EDITOR
void UMetaRoadBakeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Persist edits (the panel edits the CDO).
	SaveConfig();
}
#endif
