/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadMeshBuild/MetaRoadBuildSettingsBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaRoadBuildSettingsBase)

#if WITH_EDITOR
void UMetaRoadBuildSettingsBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnModified.Broadcast(this, PropertyChangedEvent.Property);
}
#endif
